/*
 * First go at a Voter driver. 
 *
 * Designed to handle three artificial potential drivers.
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
class VoterADriver : public ThreadedDriver
{
public:
    
  // Constructor; need that
  VoterADriver(ConfigFile* cf, int section);

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
  

  // Underlying position2d from art_pots
  
  void DoOneUpdate();

  // Commands for the position device
  void PutCommand( double speed, double turnrate );

  // Check for new commands
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &);

  // Devices provided
  player_devaddr_t position_id;
  // Redundant devices provided
  player_devaddr_t out_laser_2;
  player_devaddr_t out_position2d_2;
  player_devaddr_t out_laser_3;
  player_devaddr_t out_position2d_3;
  player_devaddr_t out_laser_4;
  player_devaddr_t out_position2d_4;


  // Required devices (odometry and laser)
  // data back from redundant devices
  // Do these need device pointers?
  Device *in_cmd_5;
  player_devaddr_t in_position2d_5;
  Device *in_cmd_6;
  player_devaddr_t in_position2d_6;
  Device *in_cmd_7;
  player_devaddr_t in_position2d_7;

  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr;

  double odom_pose[3];
  double odom_vel[3];
  int odom_stall;

  // Laser Device info
  Device *laser;
  player_devaddr_t laser_addr;
  int laser_count;
  double laser_ranges[361];

  // Control velocity
  double con_vel[3];

  // Should have your art_pot specific code here...
  bool active_goal;
  double goal_x, goal_y, goal_t;
  int cmd_state, cmd_type;
};

// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* 
VoterADriver_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new VoterADriver(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void VoterADriver_Register(DriverTable* table)
{
  table->AddDriver("voteradriver", VoterADriver_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
VoterADriver::VoterADriver(ConfigFile* cf, int section)
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
  memset(&(this->out_laser_2), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_laser_2), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_1") == 0) {
    if (this->AddInterface(this->out_laser_2) != 0) {
      this->SetError(-1);
    }
  }
  memset(&(this->out_laser_3), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_laser_3), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_2") == 0) {
    if (this->AddInterface(this->out_laser_3) != 0) {
      this->SetError(-1);
    }
  }
  memset(&(this->out_laser_4), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_laser_4), section, "provides",
			 PLAYER_LASER_CODE, -1, "rep_3") == 0) {
    if (this->AddInterface(this->out_laser_4) != 0) {
      this->SetError(-1);
    }
  }

  // Check for 3 position2d provided
  memset(&(this->out_position2d_2), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_position2d_2), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_1") == 0) {
    if (this->AddInterface(this->out_position2d_2) != 0) {
      puts("Yup... looks like this be the place of the error.");
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->out_position2d_3), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_position2d_3), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_2") == 0) {
    if (this->AddInterface(this->out_position2d_3) != 0) {
      this->SetError(-1);
      return;
    }
  }
  memset(&(this->out_position2d_4), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->out_position2d_4), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, "rep_3") == 0) {
    if (this->AddInterface(this->out_position2d_4) != 0) {
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

  // The three commands from the redundant art pots.
  this->in_cmd_5 = NULL;
  memset(&(this->in_position2d_5), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->in_position2d_5), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "rep_1") != 0) {
    PLAYER_ERROR("Could not find required Position2d_5 device!");
    this->SetError(-1);
    return;
  }
  this->in_cmd_6 = NULL;
  memset(&(this->in_position2d_6), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->in_position2d_6), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "rep_2") != 0) {
    PLAYER_ERROR("Could not find required Position2d_6 device!");
    this->SetError(-1);
    return;
  }
  this->in_cmd_7 = NULL;
  memset(&(this->in_position2d_7), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->in_position2d_7), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, "rep_3") != 0) {
    PLAYER_ERROR("Could not find required Position2d_7 device!");
    this->SetError(-1);
    return;
  }

  return;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int VoterADriver::MainSetup()
{   
  puts("Voter A driver initialising in MainSetup");
  this->active_goal = false;
  this->goal_x = this->goal_y = this->goal_t = 0;

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0)
    return -1;

  // Initialize the laser
  if (this->laser_addr.interf && this->SetupLaser() != 0)
    return -1;

  // TODO: Initialize the position devices from the redundant art pots.

  puts("Voter A driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
void VoterADriver::MainQuit()
{
  puts("Shutting Voter A driver down");

  if(this->laser)
    this->ShutdownLaser();

  puts("Voter A driver has been shutdown");
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int VoterADriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data)
{
  puts("VoterADriver ProcessMessage");
  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
			   PLAYER_POSITION2D_DATA_STATE, this->odom_addr)) {
    assert(hdr->size == sizeof(player_position2d_data_t));
    // TODO: Need to figure which device it is from
    ProcessOdom(hdr, *reinterpret_cast<player_position2d_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_LASER_DATA_SCAN, this->laser_addr)) {
    ProcessLaser(*reinterpret_cast<player_laser_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_POS,
				  this->position_id)) {
    assert(hdr->size == sizeof(player_position2d_cmd_pos_t));
    ProcessCommand(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
                                PLAYER_POSITION2D_CMD_VEL,
                                this->position_id)) {
    assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
    // make a copy of the header and change the address
    player_msghdr_t newhdr = *hdr;
    newhdr.addr = this->odom_addr;
    this->odom->PutMsg(this->InQueue, &newhdr, (void*)data);
    this->cmd_type = 0;
    this->active_goal = false;

    return 0;
  } else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, -1, this->position_id)) {
    // Pass the request on to the underlying position device and wait for
    // the reply.
    Message* msg;

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
    return -1;
  }
}



////////////////////////////////////////////////////////////////////////////////
// Main function for device thread
void VoterADriver::Main() 
{
  // TODO... eh...
  // The main loop; interact with the device here
  for(;;)
  {
    // test if we are supposed to cancel
    puts("VoterADriver MainLoop");
    this->Wait();
    pthread_testcancel();
    this->DoOneUpdate();
    // Sleep (you might, for example, block on a read() instead)
    //usleep(100000);

  }
}

void VoterADriver::DoOneUpdate() {
  if (this->InQueue->Empty()) {
    return;
  }

  this->ProcessMessages();

  if (!this->active_goal) {
    return;
  }
}


////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("Voter A driver initializing");
    VoterADriver_Register(table);
    puts("Voter A driver done");
    return(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the underlying odom device.
int VoterADriver::ShutdownOdom()
{

  // Stop the robot before unsubscribing
  this->PutCommand(0, 0);

  this->odom->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the laser
int VoterADriver::ShutdownLaser()
{
  this->laser->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int VoterADriver::SetupOdom()
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

  this->odom_pose[0] = this->odom_pose[1] = this->odom_pose[2] = 0.0;
  this->odom_vel[0] = this->odom_vel[1] = this->odom_vel[2] = 0.0;
  this->cmd_state = 1;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the laser
int VoterADriver::SetupLaser()
{
  if(!(this->laser = deviceTable->GetDevice(this->laser_addr))) {
    PLAYER_ERROR("unable to locate suitable laser device");
    return -1;
  }
  if (this->laser->Subscribe(this->InQueue) != 0) {
    PLAYER_ERROR("unable to subscribe to laser device");
    return -1;
  }

  this->laser_count = 0;
  //this->laser_ranges = NULL;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Process new odometry data
void VoterADriver::ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data)
{

  // Cache the new odometric pose, velocity, and stall info
  // NOTE: this->odom_pose is in (mm,mm,deg), as doubles
  this->odom_pose[0] = data.pos.px; // * 1e3;
  this->odom_pose[1] = data.pos.py; // * 1e3;
  this->odom_pose[2] = data.pos.pa; //RTOD(data.pos.pa);
  this->odom_vel[0] = data.vel.px; // * 1e3;
  this->odom_vel[1] = data.vel.py; // * 1e3;
  this->odom_vel[2] = data.vel.pa; //RTOD(data.vel.pa);
  this->odom_stall = data.stall;

  // Also change this info out for use by others
  player_msghdr_t newhdr = *hdr;
  newhdr.addr = this->position_id;
  this->Publish(&newhdr, (void*)&data);
}

////////////////////////////////////////////////////////////////////////////////
// Process laser data
void VoterADriver::ProcessLaser(player_laser_data_t &data)
{
  int i;
  
  laser_count = data.ranges_count;
  for (i = 0; i < data.ranges_count; i++) {
    laser_ranges[i] = data.ranges[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void VoterADriver::PutCommand(double cmd_speed, double cmd_turnrate)
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
void VoterADriver::ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd)
{
  this->cmd_type = 1;
  this->cmd_state = cmd.state;

  if((cmd.pos.px != this->goal_x) || (cmd.pos.py != this->goal_y) || (cmd.pos.pa != this->goal_t))
  {
    this->active_goal = true;
    this->goal_x = cmd.pos.px;
    this->goal_y = cmd.pos.py;
    this->goal_t = cmd.pos.pa;
  }
}

