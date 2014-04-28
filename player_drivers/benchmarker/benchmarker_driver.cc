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

#include "../../include/taslimited.h"


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

  // Set up the underlying odometry device
  int SetupOdom();
  int ShutdownOdom();
  void ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data);

  // Set up the ranger device
  int SetupRanger();
  int ShutdownRanger();
  void ProcessRanger(player_ranger_data_range_t &);

  // Set up the required position2ds
  void ProcessVelCmdFromVoter(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_number);

  void DoOneUpdate();

  // Commands for the position device
  void PutCommand(double speed, double turnrate);
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd);

  void SendWaypoints(QueuePointer & resp_queue);

  // Devices provided
  player_devaddr_t replicate_odom; // "actual:localhost:6666:position2d:10"
  player_devaddr_t replicate_rangers; // "actual:localhost:6666:ranger:10"
  player_devaddr_t cmd_planner;
  player_devaddr_t cmd_out_odom; // "original:localhost:6666:position2d:1"

  // Required devices (odometry and ranger)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr; // "original:localhost:6666:position2d:0"

  // Odometry Device to Voter
  Device *odom_voter;
  player_devaddr_t odom_voter_addr; // "original:localhost:6666:position2d:0"

  // Ranger Device info
  double ranger_last_timestamp;
  int ranger_count;
  Device *ranger;
  player_devaddr_t ranger_addr; // "original:localhost:6666:ranger:0"

  double curr_goal_x, curr_goal_y, curr_goal_a; // Current goal for planners

  // TAS Stuff
  pid_t pid;
  int priority;
  cpu_speed_t cpu_speed;
  cpu_id_t cpu;

  // timing
  timestamp_t last;
  realtime_t elapsed_time_seconds;
};

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
  // Check for position2d (we provide)
  memset(&(this->replicate_odom), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicate_odom), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "actual") == 0) {
    if (this->AddInterface(this->replicate_odom) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for provided ranger
  memset(&(this->replicate_rangers), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicate_rangers), section, "provides",
			 PLAYER_RANGER_CODE, -1, "actual") == 0) {
    if (this->AddInterface(this->replicate_rangers) != 0) {
      this->SetError(-1);
    }
  }

  // Check for planner for commands to the replicas
  memset(&(this->cmd_planner), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->cmd_planner), section, "provides",
			   PLAYER_PLANNER_CODE, -1, "actual") == 0) {
    if (this->AddInterface(this->cmd_planner) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for position2d for commands
  memset(&(this->cmd_out_odom), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->cmd_out_odom), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "original") == 0) {
    if (this->AddInterface(this->cmd_out_odom) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for position2d (we require)
  this->odom = NULL;
  // TODO: No memset for the odom? -jcm
  if (cf->ReadDeviceAddr(&(this->odom_addr), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "original") != 0) {
    PLAYER_ERROR("Could not find required position2d device!");
    this->SetError(-1);
    return;
  }

  // Check for position2d to the voter
  this->odom_voter = NULL;
  if (cf->ReadDeviceAddr(&(this->odom_voter_addr), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "actual") != 0) {
    PLAYER_ERROR("Could not find required position2d device: odom_voter");
    this->SetError(-1);
    return;
  }

  // RANGER!
  this->ranger = NULL;
  memset(&(this->ranger_addr), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->ranger_addr), section, "requires",
			 PLAYER_RANGER_CODE, -1, "original") != 0) {
    PLAYER_ERROR("Could not find required ranger device!");
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

  InitTAS(3, &cpu_speed);

  ranger_count = 0;
  ranger_last_timestamp = 0.0;

  this->curr_goal_x = this->curr_goal_y = this->curr_goal_a = 0;

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0) {
    return -1;
  }

  // Initialize the ranger
  if (this->ranger_addr.interf && this->SetupRanger() != 0) {
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

  if(this->ranger)
    this->ShutdownRanger();

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
				  PLAYER_RANGER_DATA_RANGE, this->ranger_addr)) {
    // Ranger scan update; update scan data
    ProcessRanger(*reinterpret_cast<player_ranger_data_range_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_RANGER_DATA_INTNS, this->ranger_addr)) {
    // we are ignoring the intensity values for now
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_POS,
				  this->cmd_out_odom)) {
    assert(hdr->size == sizeof(player_position2d_cmd_pos_t));
    ProcessCommand(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *> (data));
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
			     PLAYER_POSITION2D_CMD_VEL,
			     this->replicate_odom)) {
    // New command velocity from voter
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    ProcessVelCmdFromVoter(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *> (data), index);
    return 0;
  } else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
				   PLAYER_PLANNER_REQ_GET_WAYPOINTS,
				   this->cmd_planner)) {
    SendWaypoints(resp_queue);
    return(0);
  } else {
    puts("Benchmarker: I don't know what to do with that.");
    // Message not dealt with with
    return -1;
  }
}

void BenchmarkerDriver::SendWaypoints(QueuePointer & resp_queue) {
  player_planner_waypoints_req_t reply;

  reply.waypoints_count = 1;
  reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
  reply.waypoints[0].px = curr_goal_x;
  reply.waypoints[0].py = curr_goal_y;
  reply.waypoints[0].pa = curr_goal_a;

  this->Publish(this->cmd_planner, resp_queue,
		PLAYER_MSGTYPE_RESP_ACK,
		PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		(void*)&reply);
  free(reply.waypoints);
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
  this->odom_voter->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the ranger
int BenchmarkerDriver::ShutdownRanger()
{
  this->ranger->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int BenchmarkerDriver::SetupOdom()
{
  if(!(this->odom = deviceTable->GetDevice(this->odom_addr)))
  {
    PLAYER_ERROR("ODOM: unable to locate suitable position device");
    return -1;
  }
  if(this->odom->Subscribe(this->InQueue) != 0)
  {
    PLAYER_ERROR("ODOM: unable to subscribe to position device");
    return -1;
  }

  if(!(this->odom_voter = deviceTable->GetDevice(this->odom_voter_addr)))
  {
    PLAYER_ERROR("ODOM_VOTER: unable to locate suitable position device");
    //    return -1;
    // art pot controller uses planner instead of odom_voter
  }
  if(this->odom_voter->Subscribe(this->InQueue) != 0)
  {
    PLAYER_ERROR("ODOM_VOTER: unable to subscribe to position device");
    //    return -1;
  }


  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the ranger
int BenchmarkerDriver::SetupRanger()
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
void BenchmarkerDriver::ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data)
{
  // Also change this info out for use by others
  //  player_msghdr_t newhdr = *hdr;
  //  newhdr.addr = this->replicate_odom;
  //  this->Publish(&newhdr, (void*)&data);

  this->Publish(this->replicate_odom,
		PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE,
		(void*)&data, 0, NULL, true);
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void BenchmarkerDriver::ProcessRanger(player_ranger_data_range_t &data)
{
  // Set timer
  puts("TIMER START");
  this->Publish(this->replicate_rangers,
		PLAYER_MSGTYPE_DATA, PLAYER_RANGER_DATA_RANGE,
		(void*)&data, 0, NULL, true);
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
// This is the output from the replicas, so vote on it.
void BenchmarkerDriver::ProcessVelCmdFromVoter(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_num) {
  double cmd_vel = 0.0;
  double cmd_rot_vel = 0.0;

  // Stop timer and report
  puts("TIMER STOP");

  cmd_vel = cmd.vel.px;
  cmd_rot_vel = cmd.vel.pa;

  this->PutCommand(cmd_vel, cmd_rot_vel);
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

void BenchmarkerDriver::ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd) {
  curr_goal_x = cmd.pos.px;
  curr_goal_y = cmd.pos.py;
  curr_goal_a = cmd.pos.pa;

  if (this->odom_voter != NULL) {
    this->odom_voter->PutMsg(this->InQueue,
			     PLAYER_MSGTYPE_CMD,
			     PLAYER_POSITION2D_CMD_POS,
			     (void*)&cmd, sizeof(cmd), NULL);
  }
}
