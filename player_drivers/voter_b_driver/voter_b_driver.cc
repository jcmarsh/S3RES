/*
 * Second try at a Voter driver. 
 *
 * Designed to handle local navigation using three Art Pot controllers
 * 
 * Similar to (and based off of) the Player provided vfh driver
 * shared object.
 */

#include <unistd.h>
#include <string.h>
#include <math.h>

#include <libplayercore/playercore.h>

////////////////////////////////////////////////////////////////////////////////
// The class for the driver
class VoterBDriver : public ThreadedDriver
{
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

  // Set up the Art Pot position devices
  int SetupArtPotCmds();
  int ShutdownArtPotCmds();

  // Set up the required position2ds
  void ProcessVelCmdFromRep(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_number);

  // Underlying position2d from art_pots
  
  void DoOneUpdate();

  // Commands for the position device
  void PutCommand( double speed, double turnrate );

  // Check for new commands
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &);

  // Devices provided
  player_devaddr_t position_id;
  // Redundant devices provided
  player_devaddr_t replicated_laser_2;
  player_devaddr_t data_to_cmd_from_rep_position2d_2;
  player_devaddr_t cmd_to_rep_planner_2;
  player_devaddr_t replicated_laser_3;
  player_devaddr_t data_to_cmd_from_rep_position2d_3;
  player_devaddr_t cmd_to_rep_planner_3;
  player_devaddr_t replicated_laser_4;
  player_devaddr_t data_to_cmd_from_rep_position2d_4;
  player_devaddr_t cmd_to_rep_planner_4;


  // Required devices (odometry and laser)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr;

  // Laser Device info
  Device *laser;
  player_devaddr_t laser_addr;

  // Control velocity
  double con_vel[3];

  // Should have your art_pot specific code here...
  double goal_x, goal_y, goal_a;
  int cmd_state, cmd_type;
};

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
  memset(&(this->replicated_laser_2), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicated_laser_2), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_1") == 0) {
    if (this->AddInterface(this->replicated_laser_2) != 0) {
      this->SetError(-1);
    }
  }
  memset(&(this->replicated_laser_3), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicated_laser_3), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_2") == 0) {
    if (this->AddInterface(this->replicated_laser_3) != 0) {
      this->SetError(-1);
    }
  }
  memset(&(this->replicated_laser_4), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->replicated_laser_4), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_3") == 0) {
    if (this->AddInterface(this->replicated_laser_4) != 0) {
      this->SetError(-1);
    }
  }

  // Check for 3 position2d for commands from the replicas
  memset(&(this->data_to_cmd_from_rep_position2d_2), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->data_to_cmd_from_rep_position2d_2), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_1") == 0) {
    if (this->AddInterface(this->data_to_cmd_from_rep_position2d_2) != 0) {
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->data_to_cmd_from_rep_position2d_3), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->data_to_cmd_from_rep_position2d_3), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_2") == 0) {
    if (this->AddInterface(this->data_to_cmd_from_rep_position2d_3) != 0) {
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->data_to_cmd_from_rep_position2d_4), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->data_to_cmd_from_rep_position2d_4), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_3") == 0) {
    if (this->AddInterface(this->data_to_cmd_from_rep_position2d_4) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for 3 planner for commands to the replicas
  memset(&(this->cmd_to_rep_planner_2), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->cmd_to_rep_planner_2), section, "provides",
			 PLAYER_PLANNER_CODE, -1, "rep_1") == 0) {
    if (this->AddInterface(this->cmd_to_rep_planner_2) != 0) {
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->cmd_to_rep_planner_3), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->cmd_to_rep_planner_3), section, "provides",
			 PLAYER_PLANNER_CODE, -1, "rep_2") == 0) {
    if (this->AddInterface(this->cmd_to_rep_planner_3) != 0) {
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->cmd_to_rep_planner_4), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->cmd_to_rep_planner_4), section, "provides",
			 PLAYER_PLANNER_CODE, -1, "rep_3") == 0) {
    if (this->AddInterface(this->cmd_to_rep_planner_4) != 0) {
      this->SetError(-1);
      return;
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

  // Let's try to launch the replicas


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
    this->cmd_type = 0;

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
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
			       PLAYER_PLANNER_REQ_GET_WAYPOINTS,
			       this->cmd_to_rep_planner_2)) {
    puts("Waypoints requested");
    // TODO: No checks are made that all replicas get the same waypoints...
    //   WILL cause problems if interleaved with a command update.
    // controller (1st replica) is requesting waypoints
    // For now only one waypoint at a time (it's Art Pot, so fine.)
    player_planner_waypoints_req_t reply;

    reply.waypoints_count = 1;
    reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
    reply.waypoints[0].px = goal_x;
    reply.waypoints[0].py = goal_y;
    reply.waypoints[0].pa = goal_a;

    this->Publish(this->cmd_to_rep_planner_2, resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		  (void*)&reply);
    free(reply.waypoints);
    return(0);
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
			       PLAYER_PLANNER_REQ_GET_WAYPOINTS,
			       this->cmd_to_rep_planner_3)) {
    puts("Waypoints requested");
    // controller (2nd replica) is requesting waypoints
    // For now only one waypoint at a time (it's Art Pot, so fine.)
    player_planner_waypoints_req_t reply;

    reply.waypoints_count = 1;
    reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
    reply.waypoints[0].px = goal_x;
    reply.waypoints[0].py = goal_y;
    reply.waypoints[0].pa = goal_a;

    this->Publish(this->cmd_to_rep_planner_3, resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		  (void*)&reply);
    free(reply.waypoints);
    return(0);
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
			       PLAYER_PLANNER_REQ_GET_WAYPOINTS,
			       this->cmd_to_rep_planner_4)) {
    puts("Waypoints requested");
    // controller (3rd replica) is requesting waypoints
    // For now only one waypoint at a time (it's Art Pot, so fine.)
    player_planner_waypoints_req_t reply;

    reply.waypoints_count = 1;
    reply.waypoints = (player_pose2d_t*)malloc(sizeof(reply.waypoints[0]));
    reply.waypoints[0].px = goal_x;
    reply.waypoints[0].py = goal_y;
    reply.waypoints[0].pa = goal_a;

    this->Publish(this->cmd_to_rep_planner_4, resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_PLANNER_REQ_GET_WAYPOINTS,
		  (void*)&reply);
    free(reply.waypoints);
    return(0);
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_VEL,
				  this->data_to_cmd_from_rep_position2d_2)) {
    // New command velocity from replica 1
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    ProcessVelCmdFromRep(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *> (data), 1);
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_VEL,
				  this->data_to_cmd_from_rep_position2d_3)) {
    // New command velocity from replica 2
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    ProcessVelCmdFromRep(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *> (data), 2);
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_VEL,
				  this->data_to_cmd_from_rep_position2d_4)) {
    // New command velocity from replica 3
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    ProcessVelCmdFromRep(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *> (data), 3);
    return 0;
  } else {
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
    puts("Voter A driver initializing");
    VoterBDriver_Register(table);
    puts("Voter A driver done");
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

  this->cmd_state = 1;

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
  // Also change this info out for use by others
  player_msghdr_t newhdr = *hdr;
  newhdr.addr = this->position_id;
  this->Publish(&newhdr, (void*)&data);

  // Need to publish to the replicas
  this->Publish(this->data_to_cmd_from_rep_position2d_2,
		PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE,
		(void*)&data, 0, NULL, true);
  this->Publish(this->data_to_cmd_from_rep_position2d_3,
		PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE,
		(void*)&data, 0, NULL, true);
  this->Publish(this->data_to_cmd_from_rep_position2d_4,
		PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE,
		(void*)&data, 0, NULL, true);
}

////////////////////////////////////////////////////////////////////////////////
// Process laser data
void VoterBDriver::ProcessLaser(player_laser_data_t &data)
{
  this->Publish(this->replicated_laser_2,
		PLAYER_MSGTYPE_DATA, PLAYER_LASER_DATA_SCAN,
		(void*)&data, 0, NULL, true);
  this->Publish(this->replicated_laser_3,
		PLAYER_MSGTYPE_DATA, PLAYER_LASER_DATA_SCAN,
		(void*)&data, 0, NULL, true);
  this->Publish(this->replicated_laser_4,
		PLAYER_MSGTYPE_DATA, PLAYER_LASER_DATA_SCAN,
		(void*)&data, 0, NULL, true);
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
void VoterBDriver::ProcessVelCmdFromRep(player_msghdr_t* hdr, player_position2d_cmd_vel_t &cmd, int replica_number) {
  // TODO: Implement
  // Can use PutCommand (below)
  printf("Replica report: (%d): \t%f\t%f\n", replica_number, cmd.vel.px, cmd.vel.pa);
  if (replica_number == 1) {
    this->PutCommand(cmd.vel.px, cmd.vel.pa);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void VoterBDriver::PutCommand(double cmd_speed, double cmd_turnrate)
{
  player_position2d_cmd_vel_t cmd;

  this->con_vel[0] = cmd_speed;
  this->con_vel[1] = 0;
  this->con_vel[2] = cmd_turnrate;

  memset(&cmd, 0, sizeof(cmd));

  // Stop the robot if the motor state is set to disabled
  if (this->cmd_state == 0) {
    cmd.vel.px = 0;
    cmd.vel.py = 0;
    cmd.vel.pa = 0;
  } else { // Position mode
    cmd.vel.px = this->con_vel[0];
    cmd.vel.py = this->con_vel[1];
    cmd.vel.pa = this->con_vel[2];
  }

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

