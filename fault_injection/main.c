#include "register_util.h"

void main(int argc, char** argv) {
	// Should be simple: inject a bit flip into the process specified by argument
	pid_t attack_pid = 0;
	time_t t;
	srand((unsigned) time(&t));
	
	if (argc < 2) {
		printf("Usage: inject_error <pid>\n");
		return;
	} else {
		attack_pid = atoi(argv[1]);
		printf("Attacking pid: %d\n", attack_pid);
	}
	
	// Attach stops the process
	printf("Attaching\n");
	if (ptrace(PTRACE_ATTACH, attack_pid, NULL, NULL) < 0) {
		perror("Failed to attach");
	}
	printf("Wait for stop\n");
	waitpid(attack_pid);

	printf("Inject Error\n");
	injectRegError(attack_pid);

	printf("Resume (IF YOU CAN! HAHAHAHAHA)\n");
	if (ptrace(PTRACE_CONT, attack_pid, NULL, NULL) < 0) {
		perror("Failed to resume");
	}
}
