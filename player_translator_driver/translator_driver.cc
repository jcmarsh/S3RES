/*
 * Translates (and isolates) Player to pipes.
 * 
 * James Marshall
 */

#include <math.h> // TODO: REVIEW
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libplayercore/playercore.h>

#include "../include/taslimited.h"
#include "../include/replicas.h"
#include "../include/commtypes.h"

#define REP_COUNT 1

////////////////////////////////////////////////////////////////////////////////
// The class for the driver
class TranslatorDriver : public ThreadedDriver {
public:
  // Constructor; need that
  TranslatorDriver(ConfigFile* cf, int section);

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
  void ProcessOdom(player_position2d_data_t &data);

  // Set up the ranger device
  int SetupRanger();
  int ShutdownRanger();
  void ProcessRanger(player_ranger_data_range_t &);

  void DoOneUpdate();

  // Commands for the position device
  void PutCommand(double speed, double turnrate);
  void ProcessCommand(player_position2d_cmd_pos_t &cmd);

  void SendWaypoints();

  // Devices provided - These are how to send goals to the translator.
  player_devaddr_t cmd_out_odom; // "original:localhost:6666:position2d:1"

  // Required devices (odometry and ranger)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr; // "original:localhost:6666:position2d:0"
  double pose[3];

  // Ranger Device info
  int ranger_countdown;
  Device *ranger;
  player_devaddr_t ranger_addr; // "original:localhost:6666:ranger:0"

  double curr_goal[3]; // Current goal for planners

  // TAS Stuff
  cpu_speed_t cpu_speed;

  // Replica related data
  struct replica_group repGroup;
  struct replica replicas[REP_COUNT];
};

// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* 
TranslatorDriver_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new TranslatorDriver(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void TranslatorDriver_Register(DriverTable* table)
{
  table->AddDriver("translatordriver", TranslatorDriver_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
TranslatorDriver::TranslatorDriver(ConfigFile* cf, int section)
  : ThreadedDriver(cf, section)
{
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

  // RANGER!
  ranger_countdown = 5;
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
int TranslatorDriver::MainSetup()
{   
  int index = 0;
  int flags = 0;

  puts("Translator driver initialising in MainSetup");

  InitTAS(3, &cpu_speed, 10);
  curr_goal[INDEX_X] = curr_goal[INDEX_Y] = curr_goal[INDEX_A] = 0.0;

  // Initial starting position
  pose[INDEX_X] = -7.0;
  pose[INDEX_Y] = -7.0;
  pose[INDEX_A] = 0.0;

  // Initialize the position device we are reading from
  if (this->SetupOdom() != 0) {
    return -1;
  }

  // Initialize the ranger
  if (this->ranger_addr.interf && this->SetupRanger() != 0) {
    return -1;
  }

  // Should just be one "replica": The program running (VoterB or a controller)
  initReplicas(&repGroup, replicas, REP_COUNT);
  // TODO: Will need to set this parameter correctly
  //  forkSingleReplica(&repGroup, 0, "BenchMarker"); // TODO uncomment

  // Need this pipe not to block so that player and the outside world play nice
  //  flags = fcntl(replicas[index].fd_outof_rep[0], F_GETFL, 0);
  //  fcntl(replicas[index].fd_outof_rep[0], F_SETFL, flags | O_NONBLOCK);

  puts("Translator driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int TranslatorDriver::MainShutdown()
{
  puts("Shutting Translator driver down");

  if(this->ranger) {
    this->ShutdownRanger();
  }

  ShutdownOdom();

  puts("Translator driver has been shutdown");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Incoming message!
int TranslatorDriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data)
{
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
    ProcessCommand(*reinterpret_cast<player_position2d_cmd_pos_t *> (data));
  } else {
    puts("Translator: I don't know what to do with that.");
    // Message not dealt with with
    return -1;
  }
}

void TranslatorDriver::SendWaypoints() {
  commSendWaypoints(replicas[0].fd_into_rep[1], curr_goal[INDEX_X], curr_goal[INDEX_Y], curr_goal[INDEX_A]);
}

void TranslatorDriver::Main() {
  for(;;) {
    this->DoOneUpdate();
  }
}

// Called by player for each non-threaded driver.
void TranslatorDriver::DoOneUpdate() {
  int retval;
  struct comm_message recv_msg;

  // This read is non-blocking
  retval = read(replicas[0].fd_outof_rep[0], &recv_msg, sizeof(struct comm_message));
  if (retval > 0) {
    switch(recv_msg.type) {
    case COMM_WAY_REQ:
      //      this->SendWaypoints(); uncomment
      break;
    case COMM_MOV_CMD:
      // This read is non-blocking... whole message should be written at once to prevent interleaving
      //	this->PutCommand(recv_msg.data.m_cmd.vel_cmd[0], recv_msg.data.m_cmd.vel_cmd[1]);
      break;
    default:
      printf("ERROR: Translator can't handle comm type: %d\n", recv_msg.type);
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
    puts("Translator driver initializing");
    TranslatorDriver_Register(table);
    puts("Translator driver done");
    return(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the underlying odom device.
int TranslatorDriver::ShutdownOdom()
{
  // Stop the robot before unsubscribing
  this->PutCommand(0, 0);

  this->odom->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Shut down the ranger
int TranslatorDriver::ShutdownRanger()
{
  this->ranger->Unsubscribe(this->InQueue);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the underlying odom device.
int TranslatorDriver::SetupOdom()
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

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the ranger
int TranslatorDriver::SetupRanger()
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
void TranslatorDriver::ProcessOdom(player_position2d_data_t &data)
{
  pose[INDEX_X] = data.pos.px;
  pose[INDEX_Y] = data.pos.py;
  pose[INDEX_A] = data.pos.pa;
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void TranslatorDriver::ProcessRanger(player_ranger_data_range_t &data)
{
  if (ranger_countdown-- < 0) {
    commSendRanger(replicas[0].fd_into_rep[1], data.ranges, pose);
  } else {
    printf("Skipping this one.\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void TranslatorDriver::PutCommand(double cmd_speed, double cmd_turnrate)
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

void TranslatorDriver::ProcessCommand(player_position2d_cmd_pos_t &cmd) {
  curr_goal[INDEX_X] = cmd.pos.px;
  curr_goal[INDEX_Y] = cmd.pos.py;
  curr_goal[INDEX_A] = cmd.pos.pa;
}
