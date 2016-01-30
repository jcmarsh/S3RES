# The "Plumber" Language

The language is intended to describe a graph of communication components, and allow each one to be launched with pipes connecting them (thus the name). See `pb.l` and `pb.y` for the language itself, and `plumbing.c` and `plumbing.h` for the code that builds an internal representation and then launches the components.

## Basic Example

First lets look at a simple example: `controllers/configs/all.cfg`

    // Every component, no protection
    log := Logger 20 // Needs to be higher than A*
    l_n := ArtPot 70
    map := Mapper 60
    plan := AStar -5
    phil := Filter 80
    Bench --RANGE_POSE_DATA-> (phil)
    (phil) --RANGE_POSE_DATA-> (l_n)
    phil --RANGE_POSE_DATA-> (map)
    phil --RANGE_POSE_DATA-> log
    (map) --MAP_UPDATE-> (plan)
    (plan) --COMM_ACK-> map // Needed to detect timeout failures
    l_n --WAY_REQ-> plan
    plan --WAY_RES-> l_n
    (l_n) --MOV_CMD-> Bench

The first line stars with a comment, denoted by `//`. Comments can also come at the end of lines, as in line two.

The next five lines are the component declarations. Each starts with a variable name (always starts with a lower case letter), followed by `:=` and a number of different parameters. These are all unprotected components, so each has an executable name (starts with a capital letter) and a priority. The executable name is the name of the executable that will be launched for that component.

The rest of the lines describe the communication between the components. The line `(phil) --RANGE_POSE_DATA-> (l_n)` specifies that the `Filter` component will send messages of type `RANGE_POSE_DATA` to the `ArtPot` component. See `comm_types.h` for message types.

`Bench` is a special component that is always launched and serves as the entry and exit point for the whole system. This component is used for making timing measurements for the whole system (benchmarker... Bench).

![Components with communication channels (Pedestrian is not in the examples shown here).](docs/system.png?raw=true "Components")

## Example with Protection

`controllers/configs/all_tri.cfg` has the same components as the previous example, but runs components inside of Voters:

    // Every component run with TMR
    // Voter_Name(controller_name)Voting_Strategy, timeout priority_offset
    // in graph, () indicates that this is the timed piped.
    log := Logger 20 // Needs to be higher than A*
    l_n := VoterR(ArtPot)TMR, 400 70
    map := VoterR(Mapper)TMR, 1600 60
    plan := VoterD(AStar)TMR, 100000 1
    phil := VoterR(Filter)TMR, 400 80
    Bench --RANGE_POSE_DATA-> (phil)
    (phil) --RANGE_POSE_DATA-> (l_n)
    phil --RANGE_POSE_DATA-> (map)
    phil --RANGE_POSE_DATA-> log
    (map) --MAP_UPDATE-> (plan)
    (plan) --COMM_ACK-> map // Needed to detect timeout failures
    l_n --WAY_REQ-> plan
    plan --WAY_RES-> l_n
    (l_n) --MOV_CMD-> Bench

The component declarations are now more complicated. `map := VoterR(Mapper)TMR, 1600 60` specifies that the `Mapper` component will be launched and replicated by the voter with the executable `VoterR`. The replication strategy will be triple modular redundancy (TMR), the timeout is 1600 microseconds, and the priority is 60. `VoterR` is intended for real-time components, and `VoterD` for non real-time components.

The parenthesis in the communication section are used by the Voters to delineate channels that are timed. For example, the `Filter` component starts a timer (for `400` microseconds) when data is received from `Bench`. This timer is reset when `Filter` sends output to `ArtPot`. However, output to `Mapper` does not reset the timer. This feature is likely in flux. Thus far it seems necessary, but confusing.
