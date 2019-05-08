///                MentOS, The Mentoring Operating system project
/// @file cmd_cpuid.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "string.h"
#include "cmd_cpuid.h"

void cmd_cpuid(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	// List of features.
	const char *ecx_features[ECX_FLAGS_SIZE] = {
		"SSE3",
		"Reserved",
		"Reserved",
		"Monitor/MWAIT",
		"CPL Debug Store",
		"Virtual Machine",
		"Safer Mode",
		"Enhanced Intel SpeedStep Technology",
		"Thermal Monitor 2",
		"SSSE3",
		"L1 Context ID",
		"Reserved",
		"Reserved",
		"CMPXCHG16B",
		"xTPR Update Control",
		"Perfmon and Debug Capability",
		"Reserved",
		"Reserved",
		"DCA",
		"SSE4.1",
		"SSE4.2",
		"Reserved",
		"Reserved",
		"POPCNT"
	};

	const char *edx_features[EDX_FLAGS_SIZE] = {
		"x87 FPU",
		"Virtual 8086 Mode",
		"Debugging Extensions",
		"Page Size Extensions",
		"Time Stamp Counter",
		"RDMSR and WRMSR",
		"Physical Address Extensions",
		"Machine Check Exception",
		"CMPXCHG8B",
		"APIC On-chip",
		"Reserved",
		"SYSENTER and SYSEXIT",
		"Memory Type Range Registers",
		"PTE Global Bit",
		"Machine Check Architecture",
		"Conditional Move Instructions",
		"Page Attribute Table",
		"36-bit Page Size",
		"Processor Serial Number",
		"Reserved",
		"Debug Store",
		"Thermal Monitor and Clock Facilities",
		"Intel MMX",
		"FXSAVE and FXRSTOR",
		"SSE",
		"SSE2",
		"Self Snoop",
		"Multi-Threading",
		"TTC",
		"Reserved",
		"Pending Break Enable"
	};

	int i;
	int verbose = 0;

	// Examine possible options.
	if (argv[1] != NULL) {
		if (strcmp(argv[1], "-v") == 0) {
			verbose = 1;
		} else {
			printf("Unknown option %s\n", argv[1]);
			printf("CPUID help message\n"
				   "-v : shows verbose CPUID information\n");

			return;
		}
	}

	printf("----- CPU ID Information -----\n");
	if (strcmp(sinfo.brand_string, "Reserved") != 0) {
		printf("%s\n", sinfo.brand_string);
	}
	printf("Vendor: %s\n", sinfo.cpu_vendor);
	printf("Type: %s, Family: %x, Model: %x\n", sinfo.cpu_type,
		   sinfo.cpu_family, sinfo.cpu_model);
	printf("Apic ID: %d\n", sinfo.apic_id);

	if (verbose == 1) {
		printf("\n--- Supported features ---\n");
		for (i = 0; i < ECX_FLAGS_SIZE; i++) {
			if (sinfo.cpuid_ecx_flags[i] == 1) {
				printf("%s\n", ecx_features[i]);
			}
		}
		for (i = 0; i < EDX_FLAGS_SIZE; i++) {
			if (sinfo.cpuid_edx_flags[i] == 1) {
				printf("%s\n", edx_features[i]);
			}
		}
		printf("---------------------------\n");
	}
}
