#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "logger_stats.h"
#include "main.h"
#include "plugin.h"

static char *host	= NULL;
static int interval = 5000;
static int port		= 4441;
#define	LOGFILE		"c:\\memstatus.txt"


SERVICE_STATUS			serviceStatus;
SERVICE_STATUS_HANDLE	hStatus;

void main(int argc, char *argv[]) {

	if(argc > 1) {
		if(strncmp(argv[1], "install", 7) == 0) {
			printf("Installing loggeragent service.\n");
			installService();
		} else if(strncmp(argv[1], "remove", 6) == 0) {
			printf("Removing loggeragent service.\n");
			removeService();
		}
	}

	SERVICE_TABLE_ENTRY serviceTable[2];
	serviceTable[0].lpServiceName = "loggeragent";
	serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)serviceMain;

	serviceTable[1].lpServiceName = NULL;
	serviceTable[1].lpServiceProc = NULL;
	// Start the control dispatcher thread for our service
	StartServiceCtrlDispatcher(serviceTable);
}

void installService() {
	LPCTSTR servicePath = "%SystemRoot%\\system\\loggeragent.exe";
	
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	if(hSCM == NULL) {
		int ret = GetLastError();
		printf("Could not get handle to Services Control Manager: %d\n", ret);
		return;
	}

	SC_HANDLE hServ = CreateService(
		hSCM,
		"loggeragent",
		"Loggerithim Agent",
		STANDARD_RIGHTS_REQUIRED,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		servicePath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);
	if(hServ == NULL) {
		int bleh = GetLastError();
		printf("Could not create service: %d\n", bleh);
		return;
	}
	StartService(hServ, 0, NULL);
	CloseServiceHandle(hServ);
}

void removeService() {
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_REQUIRED);

	if(hSCM == NULL) {
		int ret = GetLastError();
		printf("Could not get handle to Services Control Manager: %d\n", ret);
		return;
	}

	SC_HANDLE hServ = OpenService(hSCM, "loggeragent", DELETE);
	if(DeleteService(hServ) == FALSE) {
		int ret = GetLastError();
		printf("Could not remove service: %d\n", ret);
		return;
	}

	CloseServiceHandle(hServ);
	return;
}

int writeToLog(char* str) {
	FILE* log;
	log = fopen(LOGFILE, "a+");
	if(log == NULL) {
		return -1;
	}
	fprintf(log, "%s\n", str);
	fclose(log);
	return 0;
}

void serviceMain(int argc, char** argv) {

	serviceStatus.dwServiceType		= SERVICE_WIN32;
	serviceStatus.dwCurrentState	= SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWin32ExitCode	= 0;
	serviceStatus.dwServiceSpecificExitCode	= 0;
	serviceStatus.dwCheckPoint		= 0;
	serviceStatus.dwWaitHint		= 0;

	hStatus = RegisterServiceCtrlHandler(
		"loggeragent",
		(LPHANDLER_FUNCTION)controlHandler
	);
	if(hStatus == (SERVICE_STATUS_HANDLE)0) {
		// Registering Control Handler failed
		return;
	}
	// Initialize service
	int error = initService();
	if(error) {
		// Initialization failed
		serviceStatus.dwCurrentState	= SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode	= -1;
		SetServiceStatus(hStatus, &serviceStatus);
		return;
	}
	

	// We report the status to SCM
	serviceStatus.dwCurrentState		= SERVICE_RUNNING;
	SetServiceStatus(hStatus, &serviceStatus);

	// The worker loop of a service
	while(serviceStatus.dwCurrentState == SERVICE_RUNNING) {
			
			struct s_logger_stats s_lstats;			

			int result = init(&s_lstats);

			if(result) {
				serviceStatus.dwCurrentState	= SERVICE_STOPPED;
				serviceStatus.dwWin32ExitCode	= -1;
				SetServiceStatus(hStatus, &serviceStatus);
				return;
			}

			char *str = (char *) malloc(255);
			sprintf(str, "<?xml version=\"1.0\"?>");
			writeToLog(str);
			sprintf(str, "<metric>");
			writeToLog(str);
			sprintf(str, " <host>%s</host>", s_lstats.hostname);
			writeToLog(str);
			sprintf(str, " <system>%s</system>", s_lstats.system);
			writeToLog(str);
			sprintf(str, " <release>%s</release>", s_lstats.release);
			writeToLog(str);
			sprintf(str, " <version>%s</release>", s_lstats.version);
			writeToLog(str);
			sprintf(str, " <timestamp>%lu</timestamp>", s_lstats.time);
			writeToLog(str);
			sprintf(str, " <uptime>%I64d</uptime>", s_lstats.uptime);
			writeToLog(str);
			sprintf(str, " <avail_real>%llu</avail_real>", s_lstats.availrealmem);
			writeToLog(str);
			sprintf(str, " <total_real>%llu</total_real>", s_lstats.totalrealmem);
			writeToLog(str);
			sprintf(str, " <shared_mem>%lu</shared_mem>", s_lstats.sharedmem);
			writeToLog(str);
			sprintf(str, " <buffer_mem>%lu</buffer_mem>", s_lstats.buffermem);
			writeToLog(str);
			sprintf(str, " <cached_mem>%lu</cached_mem>", s_lstats.cachedmem);
			writeToLog(str);
			sprintf(str, " <avail_swap>%lu</avail_swap>", s_lstats.availswap);
			writeToLog(str);
			sprintf(str, " <total_swap>%lu</total_swap>", s_lstats.totalswap);
			writeToLog(str);
			sprintf(str, " <load>");
			writeToLog(str);
			sprintf(str, "  <one>0.0</one>");
			writeToLog(str);
			sprintf(str, "  <five>0.0</five>");
			writeToLog(str);
			sprintf(str, "  <fifteen>0.0</fifteen>");
			writeToLog(str);
			sprintf(str, " </load>");
			writeToLog(str);
			sprintf(str, " <cpu_totals>");
			writeToLog(str);
			sprintf(str, "  <sys>%lu</sys>", s_lstats.cpusys);
			writeToLog(str);
			sprintf(str, "  <user>%lu</user>", s_lstats.cpuuser);
			writeToLog(str);
			sprintf(str, " </cpu_totals>");
			writeToLog(str);
			sprintf(str, "</metric>");
			writeToLog(str);
			
			Sleep(interval);
	}
	return;
}


// Service initialization
int initService() {
	int result;
	result = writeToLog("Monitoring Started.");
	return(result);
}

// Control handler function
void controlHandler(DWORD request) {
	switch(request) {
		case SERVICE_CONTROL_STOP:
			
			writeToLog("Monitoring Stopped.");
			serviceStatus.dwWin32ExitCode	= 0;
			serviceStatus.dwCurrentState	= SERVICE_STOPPED;
			SetServiceStatus(hStatus, &serviceStatus);
			return;

		case SERVICE_CONTROL_SHUTDOWN:

			writeToLog("Monitoring Stopped.");
			serviceStatus.dwWin32ExitCode	= 0;
			serviceStatus.dwCurrentState	= SERVICE_STOPPED;
			SetServiceStatus(hStatus, &serviceStatus);
			return;

		default:
			break;
	}

	// Report current status
	SetServiceStatus(hStatus, &serviceStatus);

	return;
}

void lstat_free(struct s_logger_stats s_lstats) {
	struct s_interface	*s_if;
	struct s_fs			*s_fs;
	int					i;

	free(s_lstats.hostname);
	free(s_lstats.system);
	free(s_lstats.release);
	free(s_lstats.version);
	for(i = 0; (s_if = s_lstats.interfaces[i]) != NULL; i++) {
		free(s_if->device);
		free(s_if);
	}
	for(i = 0; (s_fs = s_lstats.filesystems[i]) != NULL; i++) {
		free(s_fs->fs);
		free(s_fs->device);
		free(s_fs->mountpoint);
		free(s_fs);
	}
	free(s_lstats.interfaces);
	free(s_lstats.filesystems);
}