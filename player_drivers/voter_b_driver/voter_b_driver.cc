/*
 * Second try at a Voter driver. 
 *
 * Designed to handle local navigation using three Art Pot controllers
 */

#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <libplayercore/playercore.h>

#include "../../include/taslimited.h"
#include "../../include/statstime.h"
#include "../../include/replicas.h"
#include "../../include/commtypes.h"

#define REP_COUNT 3
#define INIT_ROUNDS 4
#define MAX_TIME_SECONDS 0.05 // Max time for voting in seconds (50 ms)

// Either waiting for replicas to vote or waiting for the next round (next ranger input).
// Or a replica has failed and recovery is needed
typedef enum {
  VOTING,
  RECOVERY,
  WAITING
} voting_status;

////////////////////////////////////////////////////////////////////////////////
// The class for the driver
class VoterBDriver : public ThreadedDriver {
public:
  // Constructor; need that
  VoterBDriver(ConfigFile* cf, int section);

  // This method will be invoked on each incoming message
  virtual int ProcessMessage(QueuePointer &resp_queue, 
			     player_msghdr * hdr,
			     void * data);
  
private:
  // Main function for device thread.
  virtual void Main();
  virtual int MainSetup();
  virtual int MainShutdown();

  //  void call_getrlimit(int id, char *name);
  //  void call_setrlimit(int id, rlim_t c, rlim_t m);

  // Set up the underlying odometry device
  int SetupOdom();
  int ShutdownOdom();
  void ProcessOdom(player_position2d_data_t &data);

  // Set up the ranger device
  int SetupRanger();
  int ShutdownRanger();
  void ProcessRanger(player_ranger_data_range_t &);

  // Set up the required position2ds
  void ProcessVelCmdFromRep(double cmd_vel_x, double cmd_vel_a, int replica_number);

  void DoOneUpdate();

  // Commands for the position device
  void PutCommand(double speed, double turnrate);

  // Check for new commands
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &);

  // Send the same waypoints as each replica requests it.
  void SendWaypoints(QueuePointer & resp_queue, int replica_num);

  // reset / init voting state.
  void ResetVotingState();

  // Replica related methods
  int ForkReplicas(struct replica_group* rg);
  int ForkSingle(struct replica_group* rg, int number);

  // Replica related data
  struct replica_group repGroup;
  struct replica replicas[REP_COUNT];

  // Devices provided
  player_devaddr_t position_id;
  // Redundant devices provided, one for each of the three replicas
  const char* rep_names[REP_COUNT];
  player_devaddr_t replicate_rangers;
  player_devaddr_t cmd_to_rep_planners[REP_COUNT];
  player_devaddr_t data_to_cmd_from_rep_position2ds[REP_COUNT];


  // Required devices (odometry and ranger)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr;

  // Ranger Device info
  int ranger_count;
  Device *ranger;
  player_devaddr_t ranger_addr;

  // The voting information and input duplication stuff could be part of the replica struct....
  // Input Duplication stuff
  bool sent[REP_COUNT];
  double curr_goal[3]; // Current goal for planners
  double next_goal[3]; // Next goal for planners

  // Voting stuff
  voting_status vote_stat;
  bool reporting[REP_COUNT];
  double cmds[REP_COUNT][2];

  // TAS Stuff
  cpu_speed_t cpu_speed;

  // timing
  timestamp_t last;
  realtime_t elapsed_time_seconds;
};

////////////////////////////////////////////////////////////////////////////////
int VoterBDriver::ForkSingle(struct replica_group* rg, int number) {
  pid_t currentPID = 0;
  char rep_num[3];
  char write_out[3]; // File descriptor rep will write to. Should survive exec()
  char read_in[3];
  char* rep_argv[] = {"art_pot_p", "127.0.0.1", "6666", rep_num, read_in, write_out, NULL};

  // Fork child
  sprintf(rep_num, "%02d", 2 + number);
  rep_argv[3] = rep_num;
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // art_pot expects something like: ./art_pot 127.0.0.1 6666 2
      // 2 matches the interface index in the .cfg file
      sprintf(read_in, "%02d", rg->replicas[number].pipefd_into_rep[0]);
      rep_argv[4] = read_in;
      sprintf(write_out, "%02d", rg->replicas[number].pipefd_outof_rep[1]);
      rep_argv[5] = write_out;
      if (-1 == execv("art_pot_p", rep_argv)) {
	perror("EXEC ERROR!");
	exit(-1);
      }
    } else { // Parent Process
      rg->replicas[number].pid = currentPID;
    }
  } else {
    printf("Fork error!\n");
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////
int VoterBDriver::ForkReplicas(struct replica_group* rg) {
  int index = 0;

  // Fork children
  for (index = 0; index < rg->num; index++) {
    this->ForkSingle(rg, index);
    // TODO: Handle possible errors
  }

  return 1;
}

// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* 
VoterBDriver_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new VoterBDriver(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void VoterBDriver_Register(DriverTable* table)
{
  table->AddDriver("voterbdriver", VoterBDriver_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
VoterBDriver::VoterBDriver(ConfigFile* cf, int section)
  : ThreadedDriver(cf, section)
{
  int index = 0;

  this->rep_names[0] = "rep_1";
  this->rep_names[1] = "rep_2";
  this->rep_names[2] = "rep_3";

  // Check for position2d (we provide)
  memset(&(this->position_id), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->position_id), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "actual") == 0) {
    if (this->AddInterface(this->position_id) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for provided ranger
  memset(&(this->replicate_rangers), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicate_rangers), section, "provides",
			 PLAYER_RANGER_CODE, -1, NULL) == 0) {
    if (this->AddInterface(this->replicate_rangers) != 0) {
      this->SetError(-1);
    }
  }

  // Check for planner for commands to the replicas
  for (index = 0; index < REP_COUNT; index++) { 
    memset(&(this->cmd_to_rep_planners[index]), 0, sizeof(player_devaddr_t));
    if (cf->ReadDeviceAddr(&(this->cmd_to_rep_planners[index]), section, "provides",
			   PLAYER_PLANNER_CODE, -1, this->rep_names[index]) == 0) {
      if (this->AddInterface(this->cmd_to_rep_planners[index]) != 0) {
	this->SetError(-1);
	return;
      }
    }
  }

  // Check for 3 position2d for commands from the replicas
  for (index = 0; index < REP_COUNT; index++) {
    memset(&(this->data_to_cmd_from_rep_position2ds[index]), 0, sizeof(player_devaddr_t));
    if (cf->ReadDeviceAddr(&(this->data_to_cmd_from_rep_position2ds[index]), section, "provides",
			   PLAYER_POSITION2D_CODE, -1, this->rep_names[index]) == 0) {
      if (this->AddInterface(this->data_to_cmd_from_rep_position2ds[index]) != 0) {
	this->SetError(-1);
	return;
      }
    }
  }

  // Check for position2d (we require)
  this->odom = NULL;
  // TODO: No memset for the odom? -jcm
  if (cf->ReadDeviceAddr(&(this->odom_addr), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "actual") != 0) {
    PLAYER_ERROR("Could not find required position2d device!");
    this->SetError(-1);
    return;
  }

  // RANGER!
  this->ranger = NULL;
  memset(&(this->ranger_addr), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->ranger_addr), section, "requires",
			 PLAYER_RANGER_CODE, -1, "actual") != 0) {
    PLAYER_ERROR("Could not find required ranger device!");
    this->SetError(-1);
    return;
  }

  return;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int VoterBDriver::MainSetup()
{   
  int index = 0;

  puts("Voter B driver initialising in MainSetup");

  InitTAS(DEFAULT_CPU, &cpu_speed);

  ranger_count = 0;

  curr_goal[INDEX_X] = curr_goal[INDEX_Y] = curr_goal[INDEX_A] = 0.0;
  next_goal[INDEX_X] = next_goal[INDEX_Y] = next_goal[INDEX_A] = 0.0;
  for (index = 0; index < REP_COUNT; index++) {
    sent[index] = false;
  }

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0) {
    return -1;
  }

  // Initialize the ranger
  if (this->ranger_addr.interf && this->SetupRanger() != 0) {
    return -1;
  }

  this->ResetVotingState();

  // Let's try to launch the replicas
  initReplicas(&repGroup, replicas, REP_COUNT);
  this->ForkReplicas(&repGroup);

  puts("Voter B driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int VoterBDriver::MainShutdown()
{
  puts("Shutting Voter B driver down");

  if(this->ranger)
    this->ShutdownRanger();

  ShutdownOdom();

  puts("Voter B driver has been shutdown");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int VoterBDriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data)
{
  timestamp_t current;
  int index = 0;

  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
			   PLAYER_POSITION2D_DATA_STATE, this->odom_addr)) {
    // Message from underlying position device; update state
    assert(hdr->size == sizeof(player_position2d_data_t));
    ProcessOdom(*reinterpret_cast<player_position2d_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_RANGER_DATA_RANGE, this->ranger_addr)) {
    // Ranger scan update; update scan data
#ifdef _STATS_RANGER_VOTE_IN_
    current = generate_timestamp();
    printf("RANGER reached voter at: %lf\n", timestamp_to_realtime(current, cpu_speed));
#endif
    ProcessRanger(*reinterpret_cast<player_ranger_data_range *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_RANGER_DATA_INTNS, this->ranger_addr)) {
    // Ignore intensty readings from the ranger
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_POS,
				  this->position_id)) {
    // Set a new goal position for the control to try to achieve.
    assert(hdr->size == sizeof(player_position2d_cmd_pos_t));
    ProcessCommand(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *> (data));
    return 0;
  } else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, -1, this->position_id)) {
    // Pass the request on to the underlying position device and wait for the reply.
    Message* msg;

    //    puts("Odom configuration command");
    if(!(msg = this->odom->Request(this->InQueue,
                                   hdr->type,
                                   hdr->subtype,
                                   (void*)data,
                                   hdr->size,
				   &hdr->timestamp))) {
      PLAYER_WARN1("failed to forward config request with subtype: %d\n",
                   hdr->subtype);
      return(-1);
    } 
    
    player_msghdr_t* rephdr = msg->GetHeader();
    void* repdata = msg->GetPayload();
    // Copy in our address and forward the response
    rephdr->addr = this->position_id;
    this->Publish(resp_queue, rephdr, repdata);
    delete msg;
    return(0);
  } else { 
    puts("VoterB: I don't know what to do with that.");
    // Message not dealt with with
    return -1;
  }
}

void VoterBDriver::Main() {
  for(;;) {
    Wait(0.0001);
    this->DoOneUpdate();
    pthread_testcancel();
  }
}

// Called by player for each non-threaded driver.
void VoterBDriver::DoOneUpdate() {
  timestamp_t current;
  int index = 0;
  int restart_id = -1;

  struct timeval tv;
  int retval = 0;

  struct comm_header hdr;
  double cmd_vel[2];

  if (vote_stat == VOTING) {
    current = generate_timestamp();
    elapsed_time_seconds += timestamp_to_realtime(current - last, cpu_speed);
  }

  if (ranger_count < INIT_ROUNDS) {
    // Have not started running yet
  } else if ((elapsed_time_seconds > MAX_TIME_SECONDS) && (vote_stat == VOTING)) {
    // Shit has gone down. Trigger a restart as needed.
    puts("ERROR replica has missed a deadline!");
    printf("elapsed_seconds: %lf\n", elapsed_time_seconds);
    vote_stat = RECOVERY;
  }
  last = current;

  if (vote_stat == RECOVERY) {
    for (index = 0; index < REP_COUNT; index++) {
      if (reporting[index] == false) {
	// This is the failed replica, restart it
	// Send a signal to the rep's friend
	restart_id = (index + (REP_COUNT - 1)) % REP_COUNT; // Plus 2 is minus 1!
	// printf("Restarting %d, %d now!\n", index, restart_id);
	kill(repGroup.replicas[restart_id].pid, SIGUSR1);
      }
    }
    elapsed_time_seconds = 0.0;
    vote_stat = WAITING;
  }

  // Check all replicas for data
  for (index = 0; index < REP_COUNT; index++) {
    retval = read(replicas[index].pipefd_outof_rep[0], &hdr, sizeof(struct comm_header));
    if (retval > 0) {
      switch (hdr.type) {
      case COMM_WAY_REQ:
	this->SendWaypoints(index);
	break;
      case COMM_MOV_CMD:
	printf("VoterB: Recieved a move command!\n");
	retval = read(replicas[index].pipefd_outof_rep[0], cmd_vel, hdr.byte_count);
	assert(retval == hdr.byte_count);
	this->ProcessVelCmdFromRep(cmd_vel[0], cmd_vel[1], index);
	break;
      default:
	printf("ERROR: VoterB can't handle comm type: %d\n", hdr.type);
      }
    }
  }

  if (this->InQueue->Empty()) {
    return;
  }

  this->ProcessMessages();
}


////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("Voter B driver initializing");
    VoterBDriver_Register(table);
    puts("Voter B driver done");
    return(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the underlying odom device.
int VoterBDriver::ShutdownOdom()
{
  // Stop the robot before unsubscribing
  this->PutCommand(0, 0);

  this->odom->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the ranger
int VoterBDriver::ShutdownRanger()
{
  this->ranger->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int VoterBDriver::SetupOdom()
{
  if(!(this->odom = deviceTable->GetDevice(this->odom_addr)))
  {
    PLAYER_ERROR("unable to locate suitable position device");
    return -1;
  }
  if(this->odom->Subscribe(this->InQueue) != 0)
  {
    PLAYER_ERROR("unable to subscribe to position device");
    return -1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the ranger
int VoterBDriver::SetupRanger()
{
  if(!(this->ranger = deviceTable->GetDevice(this->ranger_addr))) {
    PLAYER_ERROR("unable to locate suitable ranger device");
    return -1;
  }
  if (this->ranger->Subscribe(this->InQueue) != 0) {
    PLAYER_ERROR("unable to subscribe to ranger device");
    return -1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Process new odometry data
void VoterBDriver::ProcessOdom(player_position2d_data_t &data)
{
  int index = 0;
  struct comm_header hdr;
  double pose[3];

  pose[INDEX_X] = data.pos.px;
  pose[INDEX_Y] = data.pos.py;
  pose[INDEX_A] = data.pos.pa;

  // Need to publish to the replicas
  for (index = 0; index < REP_COUNT; index++) {
    hdr.type = COMM_POS_DATA;
    hdr.byte_count = 3 * sizeof(double);
    write(replicas[index].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));

    write(replicas[index].pipefd_into_rep[1], (void*)(pose), hdr.byte_count);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void VoterBDriver::ProcessRanger(player_ranger_data_range_t &data)
{
  int index = 0;
  timestamp_t current;
  struct comm_header hdr;

  // Ignore first ranger update (to give everything a chance to init)
  if (ranger_count < INIT_ROUNDS) {
    //    puts("Ignore first few rangers");
    ranger_count++;
  } else {
    vote_stat = VOTING;
    current = generate_timestamp();
    last = current;
    
    //
    for (index = 0; index < REP_COUNT; index++) {
      // Write header
      hdr.type = COMM_RANGE_DATA;
      hdr.byte_count = data.ranges_count * sizeof(double);
      write(replicas[index].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));

      // write each of the ranges
      write(replicas[index].pipefd_into_rep[1], (void*)(data.ranges), hdr.byte_count);
    }
#ifdef _STATS_RANGER_VOTE_OUT_
    printf("RANGER left vote at: %lf\n", timestamp_to_realtime(current, cpu_speed));
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void VoterBDriver::ResetVotingState() {
  int i = 0;
  vote_stat = WAITING;
  elapsed_time_seconds = 0.0;

  for (i = 0; i < REP_COUNT; i++) {
    reporting[i] = false;
    cmds[i][0] = 0.0;
    cmds[i][1] = 0.0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// handle the request for inputs
// This is the primary input to the replicas, so make sure it is duplicated
void VoterBDriver::SendWaypoints(int replica_num) {
  int index = 0;
  bool all_sent = true;
  struct comm_header hdr;

  // For now only one waypoint at a time (it's Art Pot, so fine.)
 
  // if replica already has latest... errors
  if (sent[replica_num] == true) {
    puts("SEND WAYPOINT ERROR: requester already has latest points.");
    return;
  } else { // send and mark sent
    sent[replica_num] = true;

    hdr.type = COMM_WAY_RES;
    hdr.byte_count = 3 * sizeof(double);

    write(replicas[replica_num].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));

    write(replicas[replica_num].pipefd_into_rep[1], (void*)(curr_goal), hdr.byte_count);
  }

  // if all 3 sent, reset and move next to current
  for (index = 0; index < REP_COUNT; index++) {
    all_sent = all_sent && sent[index];
  }
  if (all_sent) {
    curr_goal[INDEX_X] = next_goal[INDEX_X];
    curr_goal[INDEX_Y] = next_goal[INDEX_Y];
    curr_goal[INDEX_A] = next_goal[INDEX_A];
    for (index = 0; index < REP_COUNT; index++) {
      sent[index] = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
// This is the output from the replicas, so vote on it.
void VoterBDriver::ProcessVelCmdFromRep(double cmd_vel_x, double cmd_vel_a, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;
  double cmd_vel = 0.0;
  double cmd_rot_vel = 0.0;

  printf("VOTE rep: %d - %f\t%f\n", replica_num, cmd_vel_x, cmd_vel_a);
  
  if (reporting[replica_num] == true) {
    // If vote is same as previous, then ignore.
    if ((cmds[replica_num][0] == cmd_vel_x) &&
    	(cmds[replica_num][1] == cmd_vel_a)) {
      // Ignore
    } else {
      puts("PROBLEMS VOTING");
    }
  } else {
    // record vote
    reporting[replica_num] = true;
    cmds[replica_num][0] = cmd_vel_x;
    cmds[replica_num][1] = cmd_vel_a;
  }
 
  cmd_vel = cmds[0][0];
  cmd_rot_vel = cmds[0][1];
  for (index = 0; index < REP_COUNT; index++) {
    // Check that all have reported
    all_reporting = all_reporting && reporting[index];

    // Check that all agree
    if (cmd_vel == cmds[index][0] && cmd_rot_vel == cmds[index][1]) {
      // all_agree stays true
    } else {
      all_agree = false;
    }
  }

  if (all_reporting && all_agree) {
    this->PutCommand(cmd_vel, cmd_rot_vel);
    ResetVotingState();
  } else if (all_reporting) {
    puts("VOTING ERROR: Not all votes agree");
    for (index = 0; index < REP_COUNT; index++) {
      printf("\t Vote %d: (%f, %f)\n", index, cmds[index][0], cmds[index][1]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void VoterBDriver::PutCommand(double cmd_speed, double cmd_turnrate)
{
  player_position2d_cmd_vel_t cmd;

  memset(&cmd, 0, sizeof(cmd));

  cmd.vel.px = cmd_speed;
  cmd.vel.py = 0;
  cmd.vel.pa = cmd_turnrate;

  this->odom->PutMsg(this->InQueue,
		     PLAYER_MSGTYPE_CMD,
		     PLAYER_POSITION2D_CMD_VEL,
		     (void*)&cmd, sizeof(cmd), NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Check for new commands from the server
void VoterBDriver::ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd)
{
  bool all_sent = true;
  bool non_sent = false;
  int index = 0;

  next_goal[INDEX_X] = cmd.pos.px;
  next_goal[INDEX_Y] = cmd.pos.py;
  next_goal[INDEX_A] = cmd.pos.pa;

  // if all three are waiting, move to current
  for (index = 0; index < REP_COUNT; index++) {
    all_sent = all_sent && sent[index];
    non_sent = non_sent || sent[index];
  }
  if (all_sent || !non_sent) {
    curr_goal[INDEX_X] = next_goal[INDEX_X];
    curr_goal[INDEX_Y] = next_goal[INDEX_Y];
    curr_goal[INDEX_A] = next_goal[INDEX_A];
  } 
}

