#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "logger_stats.h"
#include "main.h"

int init(struct s_logger_stats *s_lstats) {

	DWORD buff = 256;

	s_lstats->hostname = (char *) malloc(255);
	GetComputerName(s_lstats->hostname, &buff);
	s_lstats->system = "Windows";

	time(&s_lstats->time);
	
	HQUERY		hQuery = NULL;
	HCOUNTER	hCounter;
	char		sCounterPath[1024];
	PDH_FMT_COUNTERVALUE	value;

	sprintf(sCounterPath, "\\\\%s\\System\\System Up Time", s_lstats->hostname);
	
	PdhOpenQuery(0, 0, &hQuery);
	PdhAddCounter(hQuery, sCounterPath, 0, &hCounter);
	PdhCollectQueryData(hQuery);
	PdhGetFormattedCounterValue(hCounter, PDH_FMT_LARGE, NULL, &value);
	PdhCloseQuery(&hQuery);
	s_lstats->uptime = value.largeValue;

	sprintf(sCounterPath, "\\\\%s\\Processor\\% Privileged Time", s_lstats->hostname);
	
	PdhOpenQuery(0, 0, &hQuery);
	PdhAddCounter(hQuery, sCounterPath, 0, &hCounter);
	PdhCollectQueryData(hQuery);
	PdhGetFormattedCounterValue(hCounter, PDH_FMT_LARGE, NULL, &value);
	PdhCloseQuery(&hQuery);
	s_lstats->cpusys = value.largeValue;

	/* Get information about the OS */

	MEMORYSTATUS memory;
	GlobalMemoryStatus(&memory);
	s_lstats->totalrealmem = memory.dwTotalPhys / 1024;
	s_lstats->availrealmem = memory.dwAvailPhys / 1024;
	s_lstats->sharedmem = 0;
	s_lstats->buffermem = 0;
	s_lstats->cachedmem = 0;
	s_lstats->totalswap = memory.dwTotalPageFile / 1024;
	s_lstats->availswap = memory.dwAvailPageFile / 1024;

	OSVERSIONINFO osVersion;
	osVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osVersion);

	s_lstats->release = (char *) malloc(255);
	s_lstats->version = (char *) malloc(255);

	switch(osVersion.dwPlatformId) {

		case VER_PLATFORM_WIN32s:
			sprintf(s_lstats->system, "Windows %d.%d", osVersion.dwMajorVersion, osVersion.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			if(osVersion.dwMinorVersion == 0) {
				s_lstats->release = "Windows 95";
			} else if(osVersion.dwMinorVersion == 10) {
				s_lstats->release = "Windows 98";
			} else if(osVersion.dwMinorVersion == 90) {
				s_lstats->release = "Windows Me";
			}
			break;
		case VER_PLATFORM_WIN32_NT:
			if(osVersion.dwMajorVersion == 5 && osVersion.dwMinorVersion == 0) {
				sprintf(s_lstats->release, "Windows 2000");
				sprintf(s_lstats->version, "%s", osVersion.szCSDVersion);
			} else if(osVersion.dwMajorVersion == 5 && osVersion.dwMinorVersion == 1) {
				sprintf(s_lstats->release, "Windows XP");
				sprintf(s_lstats->version, "%s", osVersion.szCSDVersion);
			} else if(osVersion.dwMajorVersion <= 4) {
				sprintf(s_lstats->release, "Windows NT");
				sprintf(s_lstats->version, "%d.%d, %s", osVersion.dwMajorVersion, osVersion.dwMinorVersion, osVersion.szCSDVersion);
			} else {
				sprintf(s_lstats->release, "Unknown");
				sprintf(s_lstats->version, "%d.%d", osVersion.dwMajorVersion, osVersion.dwMinorVersion);
			}
			break;
	}

	return 0;
}