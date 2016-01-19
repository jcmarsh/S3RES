/*
 * Trying to run ArtPot in the FAIL* framework.
 * First attempt is just the main command loop.
 */

#include <math.h>
#include <stdbool.h>

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

#define GRID_NUM 80
#define MAP_SIZE 16
#define OFFSET_X  8
#define OFFSET_Y  8

#define VEL_SCALE 2
#define DIST_EPSILON .6
#define GOAL_RADIUS 0
#define GOAL_EXTENT .5
#define GOAL_SCALE 1
#define OBST_RADIUS .3
#define OBST_EXTENT .5
#define OBST_SCALE 1

// Controller state
double goal[] = {0.0, 0.0, 0.0};
double next_goal[] = {0.0, 0.0, 0.0};

// Position
double pos[3] = {-7.0, -7.0, 0.0};
int ranger_count = 16;
double ranges[16] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,
                     0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};

volatile int __dummy;
void __attribute__ ((noinline)) fail_marker();
void __attribute__ ((noinline)) fail_marker()
{
    __dummy = 100;
}

void __attribute__ ((noinline)) stop_trace();
void __attribute__ ((noinline)) stop_trace()
{
    __dummy = 100;
}

void os_main() {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y;
  double vel_cmd[2];
  int total_factors, i;
  bool request_way = false;

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2)  + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
  theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];

  if (dist <= DIST_EPSILON) { // Try next goal
    goal[INDEX_X] = next_goal[INDEX_X];
    goal[INDEX_Y] = next_goal[INDEX_Y];
    goal[INDEX_A] = next_goal[INDEX_A];

    request_way = true;

    dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2)  + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
    theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];
  }

  total_factors = 0;
  if (dist < GOAL_RADIUS) {
    v = 0;
    delta_x = 0;
    delta_y = 0;
  } else if (GOAL_RADIUS <= dist && dist <= GOAL_EXTENT + GOAL_RADIUS) {
      v = GOAL_SCALE * (dist - GOAL_RADIUS);
      delta_x = v * cos(theta);
      delta_y = v * sin(theta);
      total_factors += 1;
  } else {
    v = GOAL_SCALE; //* goal_extent;
    delta_x = v * cos(theta);
    delta_y = v * sin(theta);
    total_factors += 1;
  }
  
  if (dist > DIST_EPSILON) {
    // Makes the assumption that scans are evenly spaced around the robot.
    for (i = 0; i < ranger_count; i++) {
      // figure out location of the obstacle...
      tao = (2 * M_PI * i) / ranger_count;
      obs_x = ranges[i] * cos(tao);
      obs_y = ranges[i] * sin(tao);
      // obs.x and obs.y are relative to the robot, and I'm okay with that.
      dist = sqrt(pow(obs_x, 2) + pow(obs_y, 2));
      theta = atan2(obs_y, obs_x);
    
      if (dist <= OBST_EXTENT + OBST_RADIUS) {
        delta_x += -OBST_SCALE * (OBST_EXTENT + OBST_RADIUS - dist) * cos(theta);
        delta_y += -OBST_SCALE * (OBST_EXTENT + OBST_RADIUS - dist) * sin(theta);
        total_factors += 1;
      }
    }

    delta_x = delta_x / total_factors;
    delta_y = delta_y / total_factors;

    vel_cmd[0] = sqrt(pow(delta_x, 2) + pow(delta_y, 2));
    vel_cmd[1] = atan2(delta_y, delta_x);

    if (fabs(vel_cmd[1]) < 1) { // Only go forward if mostly pointed at goal
      vel_cmd[0] = VEL_SCALE * vel_cmd[0];
    } else {
      vel_cmd[0] = 0.0;
    }
    vel_cmd[1] = VEL_SCALE * vel_cmd[1];
  } else { // within distance epsilon. And already tried next goal.
    vel_cmd[0] = 0.0;
    vel_cmd[1] = 0.0;
  }

  // Calc output is: 0.222222, 1.570796
  // printf("Calc output is: %f, %f\n", vel_cmd[0], vel_cmd[1]);

  if ((abs(vel_cmd[0] - 0.222222) > 0.000002) ||
      (abs(vel_cmd[1] - 1.570796) > 0.000002)) {
    fail_marker();
  }

  stop_trace();
}