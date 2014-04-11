/*
 * Second try at a Voter driver. 
 *
 * Designed to handle local navigation using three Art Pot controllers
 * 
 * Similar to (and based off of) the Player provided vfh driver
 * shared object.
 */

// TODO:
// Make sure that all inputs are properly triplicated (for example, requests to get waypoints).
// Voting on outputs.

#include <unistd.h>
#include <string.h>
#include <math.h>

#include <libplayercore/playercore.h>

#define REP_COUNT 3

typedef enum {
  RUNNING,
  CRASHED,
  FINISHED
} replica_status; 

// replicas with no fds
struct replica_l {
  pid_t pid;
  int priority;
  replica_status status;
};

struct replica_group_l {
  struct replica_l* replicas;
  int num;
};

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
  virtual void MainQuit();
  
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

  // reset / init input duplication state

  // reset / init voting state.
  void ResetVotingState();

  // Replica related methods
  int InitReplicas(struct replica_group_l* rg, replica_l* reps, int num);
  int ForkReplicas(struct replica_group_l* rg);

  // Replica related data
  struct replica_group_l repGroup;
  struct replica_l replicas[REP_COUNT];

  // Devices provided
  player_devaddr_t position_id;
  // Redundant devices provided, one for each of the three replicas
  const char* rep_names[REP_COUNT];
  player_devaddr_t replicated_lasers[REP_COUNT];
  player_devaddr_t data_to_cmd_from_rep_position2ds[REP_COUNT];
  player_devaddr_t cmd_to_rep_planners[REP_COUNT];

  // Required devices (odometry and laser)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr;

  // Laser Device info
  Device *laser;
  player_devaddr_t laser_addr;

  // Goal position
  double goal_x, goal_y, goal_a;

  // Input Duplication stuff
  bool sent[REP_COUNT];

  // Voting stuff
  bool reporting[REP_COUNT];
  double cmds[REP_COUNT][2];
};

////////////////////////////////////////////////////////////////////////////////
int VoterBDriver::InitReplicas(struct replica_group_l* rg, replica_l* reps, int num) {
  int index = 0;

  rg->replicas = reps;
  rg->num = num;

  for (index = 0; index < rg->num; index++) {
    rg->replicas[index].pid = -1;
    rg->replicas[index].priority = -1;
    rg->replicas[index].status = RUNNING;
  }
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
int VoterBDriver::ForkReplicas(struct replica_group_l* rg) {
  pid_t currentPID = 0;
  int index = 0;
  char rep_num[2];
  char* rep_argv[] = {"art_pot", "127.0.0.1", "6666", rep_num, NULL};
  char* rep_envp[] = {"PATH=/home/jcmarsh/research/PINT/controllers", NULL};

  // Fork children
  for (index = 0; index < rg->num; index++) {
    sprintf(rep_num, "%d", 2 + index);
    rep_argv[3] = rep_num;
    currentPID = fork();

    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	//puts("VoterB:ForkReplicas: Child execing");
	// art_pot expects something like: ./art_pot 127.0.0.1 6666 2
	// 2 matches the interface index in the .cfg file
	if (-1 == execve("art_pot", rep_argv, rep_envp)) {
	  perror("EXEC ERROR!");
	  exit(-1);
	}
      } else { // Parent Process
	rg->replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return -1;
    }
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

  // Check for 3 lasers provided
  for (index = 0; index < REP_COUNT; index++) {
    memset(&(this->replicated_lasers[index]), 0, sizeof(player_devaddr_t));
    if (cf->ReadDeviceAddr(&(this->replicated_lasers[index]), section, "provides",
			   PLAYER_LASER_CODE, -1, this->rep_names[index]) == 0) {
      printf("Adding laser interface:\m");
      printf("\tindex: %d\trep_name: %s\n", index, this->rep_names[index]);
      if (this->AddInterface(this->replicated_lasers[index]) != 0) {
	this->SetError(-1);
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

  // Check for 3 planner for commands to the replicas
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
int VoterBDriver::MainSetup()
{   
  puts("Voter B driver initialising in MainSetup");
  this->goal_x = this->goal_y = this->goal_a = 0;

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0) {
    return -1;
  }

  // Initialize the laser
  if (this->laser_addr.interf && this->SetupLaser() != 0) {
    return -1;
  }

  this->ResetVotingState();

  puts("VoterB: Init replicas");
  this->InitReplicas(&repGroup, replicas, 3);
  // Let's try to launch the replicas
  puts("VoterB: Fork replicas");
  this->ForkReplicas(&repGroup);


  puts("Voter B driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
void VoterBDriver::MainQuit()
{
  puts("Shutting Voter B driver down");

  if(this->laser)
    this->ShutdownLaser();

  ShutdownOdom();

  puts("Voter B driver has been shutdown");
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int VoterBDriver::ProcessMessage(QueuePointer & resp_queue, 
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
    ProcessLaser(*reinterpret_cast<player_laser_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_POS,
				  this->position_id)) {
    // Set a new goal position for the control to try to achieve.
    assert(hdr->size == sizeof(player_position2d_cmd_pos_t));
    ProcessCommand(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
                                PLAYER_POSITION2D_CMD_VEL,
                                this->position_id)) {
    // Simply pass the velocity command through to the underlying position device
    //    puts("Command - Velocity received");
    // TODO: Consider removing this pass-through
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    // make a copy of the header and change the address
    player_msghdr_t newhdr = *hdr;
    newhdr.addr = this->odom_addr;
    this->odom->PutMsg(this->InQueue, &newhdr, (void*)data);

    return 0;
  } else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, -1, this->position_id)) {
    // Pass the request on to the underlying position device and wait for
    // the reply.
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

////////////////////////////////////////////////////////////////////////////////
// Main function for device thread
void VoterBDriver::Main() 
{
  for(;;)
  {
    // test if we are supposed to cancel
    this->Wait();
    pthread_testcancel();
    this->DoOneUpdate();
  }
}

void VoterBDriver::DoOneUpdate() {
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
// Shut down the laser
int VoterBDriver::ShutdownLaser()
{
  this->laser->Unsubscribe(this->InQueue);
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
// Set up the laser
int VoterBDriver::SetupLaser()
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
void VoterBDriver::ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data)
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
void VoterBDriver::ProcessLaser(player_laser_data_t &data)
{
  int index = 0;

  for (index = 0; index < REP_COUNT; index++) {
    this->Publish(this->replicated_lasers[index],
		  PLAYER_MSGTYPE_DATA, PLAYER_LASER_DATA_SCAN,
		  (void*)&data, 0, NULL, true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void VoterBDriver::ResetVotingState() {
  int i = 0;

  for (i = 0; i < 3; i++) {
    reporting[i] = false;
    cmds[i][0] = 0.0;
    cmds[i][1] = 0.0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// handle the request for inputs
// This is the primary input to the replicas, so make sure it is duplicated
void VoterBDriver::SendWaypoints(QueuePointer & resp_queue, int replica_num) {
  player_planner_waypoints_req_t reply;

    // For now only one waypoint at a time (it's Art Pot, so fine.)
  reply.waypoints_count = 1;
  reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
  reply.waypoints[0].px = goal_x;
  reply.waypoints[0].py = goal_y;
  reply.waypoints[0].pa = goal_a;

  //     this->Publish(this->cmd_to_rep_planner_3, resp_queue,
  //    this->Publish(this->cmd_to_rep_planner_4, resp_queue,

  this->Publish(this->cmd_to_rep_planners[replica_num], resp_queue,
		PLAYER_MSGTYPE_RESP_ACK,
		PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		(void*)&reply);
  free(reply.waypoints);
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
// This is the output from the replicas, so vote on it.
void VoterBDriver::ProcessVelCmdFromRep(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;
  double cmd_vel = 0.0;
  double cmd_rot_vel = 0.0;
  
  if (reporting[replica_num] == true) {
    // PROBLEM. This replica has now voted twice; likely another replica has failed.
    puts("VOTING ERROR: Double Vote");
  } else {
    // record vote
    reporting[replica_num] = true;
    cmds[replica_num][0] = cmd.vel.px;
    cmds[replica_num][1] = cmd.vel.pa;
  }
 
  cmd_vel = cmds[0][0];
  cmd_rot_vel = cmds[0][1];
  for (index = 0; index <3; index++) {
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
  goal_x = cmd.pos.px;
  goal_y = cmd.pos.py;
  goal_a = cmd.pos.pa;
}

