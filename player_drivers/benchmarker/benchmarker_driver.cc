/*
 * Interposes upon the base player setup and whatever I am trying to 
 * benchmark. For now it is specifically for art_pot / voter_b.
 */

#include <math.h> // TODO: REVIEW
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libplayercore/playercore.h>

#include "../../include/time.h"
#include "../../include/cpu.h"


////////////////////////////////////////////////////////////////////////////////
// The class for the driver
class BenchmarkerDriver : public ThreadedDriver {
public:
  // Constructor; need that
  BenchmarkerDriver(ConfigFile* cf, int section);

  // This method will be invoked on each incoming message
  virtual int ProcessMessage(QueuePointer &resp_queue, 
			     player_msghdr * hdr,
			     void * data);
  
private:
  // Main function for device thread.
  virtual void Main();
  virtual int MainSetup();
  virtual int MainShutdown();
  int InitTAS();  

  // Set up the underlying odometry device
  int SetupOdom();
  int ShutdownOdom();
  void ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data);

  // Set up the laser device
  int SetupLaser();
  int ShutdownLaser();
  void ProcessLaser(player_laser_data_t &);

  // Set up the required position2ds
  void ProcessVelCmdFromRep(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_number);

  void DoOneUpdate();

  // Commands for the position device
  void PutCommand(double speed, double turnrate);

  // Check for new commands
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &);

  // Send the same waypoints as each replica requests it.
  void SendWaypoints(QueuePointer & resp_queue, int replica_num);

  // Devices provided
  player_devaddr_t position_id;

  // Redundant devices provided, one for each of the three replicas
  player_devaddr_t replicat_lasers;
  player_devaddr_t cmd_to_rep_planners;
  player_devaddr_t data_to_cmd_from_rep_position2ds;


  // Required devices (odometry and laser)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr;

  // Laser Device info
  double laser_last_timestamp;
  int laser_count;
  Device *laser;
  player_devaddr_t laser_addr;

  // TAS Stuff
  pid_t pid;
  int priority;
  cpu_speed_t cpu_speed;
  cpu_id_t cpu;

  // timing
  timestamp_t last;
  realtime_t elapsed_time_seconds;
};

////////////////////////////////////////////////////////////////////////////////
int BenchmarkerDriver::InitTAS() {
  cpu = DEFAULT_CPU;
  pid = getpid();

  // Bind
  if( cpu_c::bind(pid, cpu) != cpu_c::ERROR_NONE ) {
    printf("(test_timer.cpp) init() failed calling cpu_c::_bind(pid,DEFAULT_CPU).\nExiting\n");
  }

  // TODO:
  // Set Realtime Scheduling

  // Test for high resolution timers? Maybe just once... no need everytime

  // * get the cpu speed *
  if( cpu_c::get_speed( cpu_speed, cpu ) != cpu_c::ERROR_NONE ) {
    printf("(test_timer.cpp) init() failed calling cpu_c::get_frequency(cpu_speed,cpu)\n" );
  }
  printf("CPU Speed: %lld\n", cpu_speed);
}

// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* 
BenchmarkerDriver_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new BenchmarkerDriver(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void BenchmarkerDriver_Register(DriverTable* table)
{
  table->AddDriver("benchmarkerdriver", BenchmarkerDriver_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
BenchmarkerDriver::BenchmarkerDriver(ConfigFile* cf, int section)
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

  // Check for provided laser
  memset(&(this->replicat_lasers), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicat_lasers), section, "provides",
			 PLAYER_LASER_CODE, -1, NULL) == 0) {
    if (this->AddInterface(this->replicat_lasers) != 0) {
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

  // LASER!
  this->laser = NULL;
  memset(&(this->laser_addr), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->laser_addr), section, "requires",
			 PLAYER_LASER_CODE, -1, "actual") != 0) {
    PLAYER_ERROR("Could not find required laser device!");
    this->SetError(-1);
    return;
  }

  return;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int BenchmarkerDriver::MainSetup()
{   
  int index = 0;

  puts("Benchmarker driver initialising in MainSetup");

  this->InitTAS();

  laser_count = 0;
  laser_last_timestamp = 0.0;

  this->curr_goal_x = this->curr_goal_y = this->curr_goal_a = 0;
  this->next_goal_x = this->next_goal_y = this->next_goal_a = 0;
  for (index = 0; index < REP_COUNT; index++) {
    sent[index] = false;
  }

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0) {
    return -1;
  }

  // Initialize the laser
  if (this->laser_addr.interf && this->SetupLaser() != 0) {
    return -1;
  }

  puts("Benchmarker driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int BenchmarkerDriver::MainShutdown()
{
  puts("Shutting Benchmarker driver down");

  if(this->laser)
    this->ShutdownLaser();

  ShutdownOdom();

  puts("Benchmarker driver has been shutdown");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int BenchmarkerDriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data)
{
  int index = 0;

  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
			   PLAYER_POSITION2D_DATA_STATE, this->odom_addr)) {
    // Message from underlying position device; update state
    assert(hdr->size == sizeof(player_position2d_data_t));
    ProcessOdom(hdr, *reinterpret_cast<player_position2d_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_LASER_DATA_SCAN, this->laser_addr)) {
    // Laser scan update; update scan data
    if (laser_last_timestamp == hdr->timestamp) {
      // Likely a duplicate message; ignore
    } else {
      laser_last_timestamp = hdr->timestamp;
      ProcessLaser(*reinterpret_cast<player_laser_data_t *> (data));
    }
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
    // Check the replica interfaces
    for (index = 0; index < REP_COUNT; index++) {
      // Check for requests for waypoints from replicas
      if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
			       PLAYER_PLANNER_REQ_GET_WAYPOINTS,
			       this->cmd_to_rep_planners[index])) {
	SendWaypoints(resp_queue,  index);
	return(0);
      }

      // Check for commands from replicas
      if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
			       PLAYER_POSITION2D_CMD_VEL,
			       this->data_to_cmd_from_rep_position2ds[index])) {
	// New command velocity from replica [index]
	assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
	ProcessVelCmdFromRep(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *> (data), index);
	return 0;
      }
    }
    
    puts("I don't know what to do with that.");
    // Message not dealt with with
    return -1;
  }
}

void BenchmarkerDriver::Main() {
  for(;;) {
    Wait(0.001);
    this->DoOneUpdate();
    pthread_testcancel();
  }
}

// Called by player for each non-threaded driver.
void BenchmarkerDriver::DoOneUpdate() {
  timestamp_t current;
  int index = 0;
  int restart_id = -1;

  if (vote_stat == VOTING) {
    current = generate_timestamp();
    elapsed_time_seconds += timestamp_to_realtime(current - last, cpu_speed);
  }

  if (laser_count < INIT_ROUNDS) {
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
    puts("Benchmarker driver initializing");
    BenchmarkerDriver_Register(table);
    puts("Benchmarker driver done");
    return(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the underlying odom device.
int BenchmarkerDriver::ShutdownOdom()
{
  // Stop the robot before unsubscribing
  this->PutCommand(0, 0);

  this->odom->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the laser
int BenchmarkerDriver::ShutdownLaser()
{
  this->laser->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int BenchmarkerDriver::SetupOdom()
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
// Set up the laser
int BenchmarkerDriver::SetupLaser()
{
  if(!(this->laser = deviceTable->GetDevice(this->laser_addr))) {
    PLAYER_ERROR("unable to locate suitable laser device");
    return -1;
  }
  if (this->laser->Subscribe(this->InQueue) != 0) {
    PLAYER_ERROR("unable to subscribe to laser device");
    return -1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Process new odometry data
void BenchmarkerDriver::ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data)
{
  int index = 0;
  // Also change this info out for use by others
  player_msghdr_t newhdr = *hdr;
  newhdr.addr = this->position_id;
  this->Publish(&newhdr, (void*)&data);

  // Need to publish to the replicas
  for (index = 0; index < REP_COUNT; index++) {
    this->Publish(this->data_to_cmd_from_rep_position2ds[index],
		  PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE,
		  (void*)&data, 0, NULL, true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process laser data
void BenchmarkerDriver::ProcessLaser(player_laser_data_t &data)
{
  int index = 0;
  timestamp_t current;

  // Ignore first laser update (to give everything a chance to init)
  if (laser_count < INIT_ROUNDS) {
    puts("Ignore first few lasers");
    laser_count++;
  } else {
    puts("New Laser Data");
    vote_stat = VOTING;
    current = generate_timestamp();
    last = current;
    
    this->Publish(this->replicat_lasers,
		  PLAYER_MSGTYPE_DATA, PLAYER_LASER_DATA_SCAN,
		  (void*)&data, 0, NULL, true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void BenchmarkerDriver::ResetVotingState() {
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
void BenchmarkerDriver::SendWaypoints(QueuePointer & resp_queue, int replica_num) {
  player_planner_waypoints_req_t reply;
  int index = 0;
  bool all_sent = true;
  // For now only one waypoint at a time (it's Art Pot, so fine.)
 
  // if replica already has latest... errors
  if (sent[replica_num] == true) {
    puts("SEND WAYPOINT ERROR: requester already has latest points.");
    return;
  } else { // send and mark sent
    sent[replica_num] = true;

    reply.waypoints_count = 1;
    reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
    reply.waypoints[0].px = curr_goal_x;
    reply.waypoints[0].py = curr_goal_y;
    reply.waypoints[0].pa = curr_goal_a;

    this->Publish(this->cmd_to_rep_planners[replica_num], resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		  (void*)&reply);
    free(reply.waypoints);
  }

  // if all 3 sent, reset and move next to current
  for (index = 0; index < REP_COUNT; index++) {
    all_sent = all_sent && sent[index];
  }
  if (all_sent) {
    curr_goal_x = next_goal_x;
    curr_goal_y = next_goal_y;
    curr_goal_a = next_goal_a;
    for (index = 0; index < REP_COUNT; index++) {
      sent[index] = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
// This is the output from the replicas, so vote on it.
void BenchmarkerDriver::ProcessVelCmdFromRep(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;
  double cmd_vel = 0.0;
  double cmd_rot_vel = 0.0;

  printf("VOTE rep: %d - %f\t%f\n", replica_num, cmd.vel.px, cmd.vel.pa);
  
  if (reporting[replica_num] == true) {
    // If vote is same as previous, then ignore.
    if ((cmds[replica_num][0] == cmd.vel.px) &&
    	(cmds[replica_num][1] == cmd.vel.pa)) {
      // Ignore
    } else {
      puts("PROBLEMS VOTING");
    }
  } else {
    // record vote
    reporting[replica_num] = true;
    cmds[replica_num][0] = cmd.vel.px;
    cmds[replica_num][1] = cmd.vel.pa;
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
void BenchmarkerDriver::PutCommand(double cmd_speed, double cmd_turnrate)
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
void BenchmarkerDriver::ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd)
{
  bool all_sent = true;
  bool non_sent = false;
  int index = 0;

  next_goal_x = cmd.pos.px;
  next_goal_y = cmd.pos.py;
  next_goal_a = cmd.pos.pa;

  // if all three are waiting, move to current
  for (index = 0; index < REP_COUNT; index++) {
    all_sent = all_sent && sent[index];
    non_sent = non_sent || sent[index];
  }
  if (all_sent || !non_sent) {
    curr_goal_x = next_goal_x;
    curr_goal_y = next_goal_y;
    curr_goal_a = next_goal_a;
  } 
}

