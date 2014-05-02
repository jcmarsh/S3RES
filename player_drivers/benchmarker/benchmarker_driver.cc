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
#include "../../include/statstime.h"
#include "../../include/replicas.h"
#include "../../include/commtypes.h"

#define REP_COUNT 1

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

  // Devices provided - These are how to send goals to the benchmarker.
  player_devaddr_t cmd_out_odom; // "original:localhost:6666:position2d:1"

  // Required devices (odometry and ranger)
  // Odometry Device info
  Device *odom;
  player_devaddr_t odom_addr; // "original:localhost:6666:position2d:0"

  // Ranger Device info
  Device *ranger;
  player_devaddr_t ranger_addr; // "original:localhost:6666:ranger:0"

  double curr_goal[3]; // Current goal for planners

  // TAS Stuff
  cpu_speed_t cpu_speed;

  // timing
  timestamp_t last;
  //  realtime_t elapsed_time_seconds;

  // Replica related data
  struct replica_group repGroup;
  struct replica replicas[REP_COUNT];
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

  // TODO: Is this moving player to cpu 3? Need to review
  InitTAS(3, &cpu_speed);

  curr_goal[INDEX_X] = curr_goal[INDEX_Y] = curr_goal[INDEX_A] = 0.0;

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
  //forkSingleReplica(&repGroup, 0, "art_pot_p");
  forkSingleReplica(&repGroup, 0, "VoterB");
  //  printf("Replica - pid: %d\tfd_in_r: %d\tfd_in_w: %d\n", repGroup.replicas[0].pid, repGroup.replicas[0].pipefd_into_rep[0], repGroup.replicas[0].pipefd_into_rep[1]);

  puts("Benchmarker driver ready");

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int BenchmarkerDriver::MainShutdown()
{
  puts("Shutting Benchmarker driver down");

  if(this->ranger) {
    this->ShutdownRanger();
  }

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
    puts("Benchmarker: I don't know what to do with that.");
    // Message not dealt with with
    return -1;
  }
}

void BenchmarkerDriver::SendWaypoints() {
  struct comm_header hdr;

  hdr.type = COMM_WAY_RES;
  hdr.byte_count = 3 * sizeof(double);

  write(replicas[0].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));
  write(replicas[0].pipefd_into_rep[1], (void*)(curr_goal), hdr.byte_count);
}

void BenchmarkerDriver::Main() {
  for(;;) {
    //    Wait(0.0001);
    this->DoOneUpdate();
    //    pthread_testcancel();
  }
}

// Called by player for each non-threaded driver.
void BenchmarkerDriver::DoOneUpdate() {
  int retval;
  struct comm_header hdr;
  double cmd_vel[2];

  // This read is non-blocking
  retval = read(replicas[0].pipefd_outof_rep[0], &hdr, sizeof(struct comm_header));
  if (retval > 0) {
    assert(retval == sizeof(struct comm_header));
    switch(hdr.type) {
    case COMM_WAY_REQ:
      this->SendWaypoints();
      break;
    case COMM_MOV_CMD:
      // This read is non-blocking... could it fail? (EAGAIN)
      retval = read(replicas[0].pipefd_outof_rep[0], cmd_vel, hdr.byte_count);
      assert(retval == hdr.byte_count);
      this->PutCommand(cmd_vel[0], cmd_vel[1]);
      break;
    default:
      printf("ERROR: Benchmarker can't handle comm type: %d\n", hdr.type);
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
void BenchmarkerDriver::ProcessOdom(player_position2d_data_t &data)
{
  struct comm_header hdr;
  double pose[3];

  pose[INDEX_X] = data.pos.px;
  pose[INDEX_Y] = data.pos.py;
  pose[INDEX_A] = data.pos.pa;

  // Need to publish to the replica
  hdr.type = COMM_POS_DATA;
  hdr.byte_count = 3 * sizeof(double);
  write(replicas[0].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));

  write(replicas[0].pipefd_into_rep[1], (void*)(pose), hdr.byte_count);
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void BenchmarkerDriver::ProcessRanger(player_ranger_data_range_t &data)
{
  int index = 0;
  struct comm_header hdr;

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
#endif // _STATS_BENCH_ROUND_TRIP_

  hdr.type = COMM_RANGE_DATA;
  hdr.byte_count = data.ranges_count * sizeof(double);
  write(replicas[0].pipefd_into_rep[1], (void*)(&hdr), sizeof(struct comm_header));

  write(replicas[0].pipefd_into_rep[1], (void*)(data.ranges), hdr.byte_count);
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void BenchmarkerDriver::PutCommand(double cmd_speed, double cmd_turnrate)
{
  player_position2d_cmd_vel_t cmd;
#ifdef _STATS_BENCH_ROUND_TRIP_
  timestamp_t current;
  current = generate_timestamp();

  printf("%lf\n", timestamp_to_realtime(current - last, cpu_speed));
#endif
  memset(&cmd, 0, sizeof(cmd));

  cmd.vel.px = cmd_speed;
  cmd.vel.py = 0;
  cmd.vel.pa = cmd_turnrate;

  this->odom->PutMsg(this->InQueue,
		     PLAYER_MSGTYPE_CMD,
		     PLAYER_POSITION2D_CMD_VEL,
		     (void*)&cmd, sizeof(cmd), NULL);
}

void BenchmarkerDriver::ProcessCommand(player_position2d_cmd_pos_t &cmd) {
  curr_goal[INDEX_X] = cmd.pos.px;
  curr_goal[INDEX_Y] = cmd.pos.py;
  curr_goal[INDEX_A] = cmd.pos.pa;
}
