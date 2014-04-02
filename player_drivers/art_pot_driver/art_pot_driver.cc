/*
 * Artificial Potential driver.
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
class ArtPotDriver : public ThreadedDriver
{
public:
    
  // Constructor; need that
  ArtPotDriver(ConfigFile* cf, int section);

  // This method will be invoked on each incoming message
  virtual int ProcessMessage(QueuePointer &resp_queue, 
			     player_msghdr * hdr,
			     void * data);
  
private:
  // Main function for device thread.
  virtual void Main();
  virtual int MainSetup();
  virtual void MainQuit();
  
  double vel_scale;
  double dist_epsilon;
  double goal_radius, goal_extent, goal_scale;
  double obstacle_radius, obstacle_extent, obstacle_scale;

  // Set up the underlying odometry device
  int SetupOdom();
  int ShutdownOdom();
  void ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data);

  // Set up the laser device
  int SetupLaser();
  int ShutdownLaser();
  void ProcessLaser(player_laser_data_t &);
  
  void DoOneUpdate();
  // Commands for the position device
  void PutCommand( double speed, double turnrate );

  // Check for new commands
  void ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &);

  // Devices provided
  player_devaddr_t position_id;

  // Required devices (odometry and laser)
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
  int speed, turnrate;
  bool active_goal;
  double goal_x, goal_y, goal_t;
  int cmd_state, cmd_type;

};

// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* 
ArtPotDriver_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new ArtPotDriver(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void ArtPotDriver_Register(DriverTable* table)
{
  table->AddDriver("artpotdriver", ArtPotDriver_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
ArtPotDriver::ArtPotDriver(ConfigFile* cf, int section)
  : ThreadedDriver(cf, section)
{
  // Check for position2d (we provide)
  memset(&(this->position_id), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->position_id), section, "provides",
			 PLAYER_POSITION2D_CODE, -1, NULL) == 0) {
    if (this->AddInterface(this->position_id) != 0) {
      this->SetError(-1);
      return;
    }
  }

  // Check for position2d (we require)
  this->odom = NULL;
  // TODO: No memset for the odom? -jcm
  if (cf->ReadDeviceAddr(&(this->odom_addr), section, "requires",
			 PLAYER_POSITION2D_CODE, -1, NULL) != 0) {
    PLAYER_ERROR("Could not find required position2d device!");
    this->SetError(-1);
    return;
  }

  this->laser = NULL;
  memset(&(this->laser_addr), 0, sizeof(player_devaddr_t));
  if (cf->ReadDeviceAddr(&(this->laser_addr), section, "requires",
			 PLAYER_LASER_CODE, -1, NULL) != 0) {
    PLAYER_ERROR("Could not find required laser device!");
    this->SetError(-1);
    return;
  }

  // Read an option from the configuration file
  this->vel_scale = cf->ReadFloat(section, "vel_scale", 4);
  this->dist_epsilon = cf->ReadFloat(section, "dist_epsilon", .1);
  this->goal_radius = cf->ReadFloat(section, "goal_radius", .1);
  this->goal_extent = cf->ReadFloat(section, "goal_extent", 2);
  this->goal_scale = cf->ReadFloat(section, "goal_scale", .5);
  this->obstacle_radius = cf->ReadFloat(section, "obstacle_radius", .0);
  this->obstacle_extent = cf->ReadFloat(section, "obstacle_extent", 1.5);
  this->obstacle_scale = cf->ReadFloat(section, "obstacle_scale", .2);
			 
  return;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int ArtPotDriver::MainSetup()
{   
  puts("Artificial Potential driver initialising");
  this->active_goal = false;
  this->goal_x = this->goal_y = this->goal_t = 0;

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0)
    return -1;

  // Initialize the laser
  if (this->laser_addr.interf && this->SetupLaser() != 0)
    return -1;

  puts("Artificial Potential driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
void ArtPotDriver::MainQuit()
{
  puts("Shutting Artificial Potential driver down");

  if(this->laser)
    this->ShutdownLaser();

  puts("Artificial Potential driver has been shutdown");
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int ArtPotDriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data)
{
  puts("ArtPot: process message");
  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
			   PLAYER_POSITION2D_DATA_STATE, this->odom_addr)) {
    puts("ArtPot: recieve new odom data");
    assert(hdr->size == sizeof(player_position2d_data_t));
    ProcessOdom(hdr, *reinterpret_cast<player_position2d_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
				  PLAYER_LASER_DATA_SCAN, this->laser_addr)) {
    puts("ArtPot: recieve new laser data");
    ProcessLaser(*reinterpret_cast<player_laser_data_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				  PLAYER_POSITION2D_CMD_POS,
				  this->position_id)) {
    puts("ArtPot: recieve new position command");
    assert(hdr->size == sizeof(player_position2d_cmd_pos_t));
    ProcessCommand(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *> (data));
    return 0;
  } else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
                                PLAYER_POSITION2D_CMD_VEL,
                                this->position_id)) {
    puts("ArtPot: recieve new velocity command");
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

    puts("ArtPot: request to the odom (over the position cmd)");

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
    puts("ArtPot: This driver can't even handle me right now.");
    return -1;
  }
}



////////////////////////////////////////////////////////////////////////////////
// Main function for device thread
void ArtPotDriver::Main() 
{
  // The main loop; interact with the device here
  for(;;)
  {
    // test if we are supposed to cancel
    this->Wait();
    pthread_testcancel();
    this->DoOneUpdate();
  }
}

void ArtPotDriver::DoOneUpdate() {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y, vel, rot_vel;
  int total_factors, i;

  if (this->InQueue->Empty()) {
    return;
  }

  this->ProcessMessages();

  if (!this->active_goal) {
    return;
  }

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal_x - odom_pose[0], 2)  + pow(goal_y - odom_pose[1], 2));
  theta = atan2(goal_y - odom_pose[1], goal_x - odom_pose[0]) - odom_pose[2];

  total_factors = 0;
  if (dist < goal_radius) {
    v = 0;
    delta_x = 0;
    delta_y = 0;
  } else if (goal_radius <= dist and dist <= goal_extent + goal_radius) {
      v = goal_scale * (dist - goal_radius);
      delta_x = v * cos(theta);
      delta_y = v * sin(theta);
      total_factors += 1;
  } else {
    v = goal_scale; //* goal_extent;
    delta_x = v * cos(theta);
    delta_y = v * sin(theta);
    total_factors += 1;
  }
  
  // TODO: Could I use goal_radius for the dist_epsilon
  // TODO: Now will not react to obstacles while at a waypoint. Even moving ones.
  if (dist > dist_epsilon) {
    for (i = 0; i < laser_count; i++) {
      // figure out location of the obstacle...
      tao = (2 * M_PI * i) / laser_count;
      obs_x = laser_ranges[i] * cos(tao);
      obs_y = laser_ranges[i] * sin(tao);
      // obs.x and obs.y are relative to the robot, and I'm okay with that.
    
      dist = sqrt(pow(obs_x, 2) + pow(obs_y, 2));
      theta = atan2(obs_y, obs_x);
    
      if (dist <= obstacle_extent + obstacle_radius) {
	delta_x += -obstacle_scale * (obstacle_extent + obstacle_radius - dist) * cos(theta);
	delta_y += -obstacle_scale * (obstacle_extent + obstacle_radius - dist) * sin(theta);
	total_factors += 1;
      }
    }

    delta_x = delta_x / total_factors;
    delta_y = delta_y / total_factors;
  
    vel = sqrt(pow(delta_x, 2) + pow(delta_y, 2));
    rot_vel = atan2(delta_y, delta_x);
    vel = vel_scale * vel * (abs(M_PI - rot_vel) / M_PI);
    rot_vel = vel_scale * rot_vel;

    // TODO: Should turn to the desired theta, goal_t
    this->PutCommand(vel, rot_vel);
  } else { // within distance epsilon. Give it up, man.
    this->PutCommand(0,0);
  }
}


////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("Artificial Potential driver initializing");
    ArtPotDriver_Register(table);
    puts("Artificial Potential driver done");
    return(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the underlying odom device.
int ArtPotDriver::ShutdownOdom()
{

  // Stop the robot before unsubscribing
  this->speed = 0;
  this->turnrate = 0;
  this->PutCommand( speed, turnrate );

  this->odom->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the laser
int ArtPotDriver::ShutdownLaser()
{
  this->laser->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int ArtPotDriver::SetupOdom()
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
int ArtPotDriver::SetupLaser()
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
void ArtPotDriver::ProcessOdom(player_msghdr_t* hdr, player_position2d_data_t &data)
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
void ArtPotDriver::ProcessLaser(player_laser_data_t &data)
{
  int i;
  
  laser_count = data.ranges_count;
  for (i = 0; i < data.ranges_count; i++) {
    laser_ranges[i] = data.ranges[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void ArtPotDriver::PutCommand(double cmd_speed, double cmd_turnrate)
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
void ArtPotDriver::ProcessCommand(player_msghdr_t* hdr, player_position2d_cmd_pos_t &cmd)
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

