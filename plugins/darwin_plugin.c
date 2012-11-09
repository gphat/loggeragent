#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mach/host_info.h>
#include <mach/host_priv.h>
#include <mach/kern_return.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/processor_set.h>
#include <mach/task.h>

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/utsname.h>
#include <sys/vmmeter.h>

#include "metricops.h"

int gather(struct s_stats *s_lstats) {
	struct utsname				s_uname;
	double						loadavg[3];
	int							mib[2], pagesize, physmem, i, fscount;
	int							fs_count = 0;
	unsigned int				pcount, tcount;
	size_t						len;
	static mach_port_t			host_priv_port;
	host_cpu_load_info_data_t	cpuload;
	mach_msg_type_number_t		count;
	kern_return_t				status;
	vm_statistics_data_t		vmstat;
	processor_set_t				*psets;
	processor_set_t				p;
	task_t						*tasks;
	struct statfs				*mntbufp;
	struct s_fs					*s_fs;

	host_priv_port = mach_host_self();

	uname(&s_uname);
	s_lstats->hostname	= strdup(s_uname.nodename);
	s_lstats->system	= strdup(s_uname.sysname);
	s_lstats->release	= strdup(s_uname.release);
	s_lstats->version	= strdup(s_uname.version);

	getloadavg(loadavg, 3);
	s_lstats->load1		= loadavg[0];
	s_lstats->load5		= loadavg[1];
	s_lstats->load15	= loadavg[2];

	time(&s_lstats->time);

	len = 4;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if(sysctl(mib, 2, &s_lstats->numcpus, &len, NULL, 0) == -1) {
		perror("sysctl, HW_NCPU");
	}

	mib[1] = HW_PAGESIZE;
	if(sysctl(mib, 2, &pagesize, &len, NULL, 0) == -1) {
		perror("sysctl, HW_PAGESIZE");
	}

	mib[1] = HW_PHYSMEM;
	if(sysctl(mib, 2, &physmem, &len, NULL, 0) == -1) {
		perror("sysctl, HW_PHYSMEM");
	}

	s_lstats->totalrealmem = physmem / 1024;

	count = HOST_CPU_LOAD_INFO_COUNT;
	status = host_statistics(host_priv_port, HOST_CPU_LOAD_INFO,
		(host_info_t)&cpuload, &count);
	if(status != KERN_SUCCESS) {
		perror("Couldn't fetch CPU stats");
	}

	s_lstats->cpuuser	= cpuload.cpu_ticks[CPU_STATE_USER];
	s_lstats->cpusys	= cpuload.cpu_ticks[CPU_STATE_SYSTEM];
	s_lstats->cpuidle	= cpuload.cpu_ticks[CPU_STATE_IDLE];
	s_lstats->cpunice	= 0;

	count = sizeof(vmstat) / sizeof(integer_t);
	status = host_statistics(host_priv_port, HOST_VM_INFO,
		(host_info_t)&vmstat, &count);
	if(status != KERN_SUCCESS) {
		perror("Couldn't fetch VM stats");
	}

	s_lstats->pagesin		= vmstat.pageins;
	s_lstats->pagesout		= vmstat.pageouts;
	s_lstats->swapsin		= 0;
	s_lstats->swapsout		= 0;
	s_lstats->availrealmem	= (vmstat.free_count * pagesize) / 1024;
	s_lstats->buffermem		= 0;
	s_lstats->sharedmem		= 0;
	s_lstats->cachedmem		= 0;
	s_lstats->contexts		= 0;

	s_lstats->processes = 0;

	status = host_processor_sets(host_priv_port, &psets, &pcount);
	if(status != KERN_SUCCESS) {
		perror("Couldn't fetch processor sets");
	}
	for(i = 0; i < pcount; i++) {
		status = host_processor_set_priv(host_priv_port, psets[i], &p);
		if(status != KERN_SUCCESS) {
			perror("Couldn't set processor set privs");
		}

		status = processor_set_tasks(p, &tasks, &tcount);
		if(status != KERN_SUCCESS) {
			perror("Couldn't set processor tasks");
		}
		s_lstats->processes += tcount;	
	}

	fscount = getmntinfo(&mntbufp, MNT_NOWAIT);
	if(fscount == 0) {
		return -1;
	}

	s_lstats->filesystems = malloc(sizeof(struct s_fs *));
	if(s_lstats->filesystems == NULL) {
		return -1;
	}

	for(i = 0; i <= fscount; i++) {
		/* Need to check for UFS and HFS */
		if(fs_count > 0) {
			s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
			if(s_lstats->filesystems == NULL) {
				return -1;
			}
		}

		/* Create a new filesystem struct each time through */
		s_fs = malloc(sizeof(struct s_fs));
		if(s_fs == NULL) {
			return -1;
		}

		s_fs->fs			= strdup(mntbufp[i].f_fstypename);
		s_fs->device		= strdup(mntbufp[i].f_mntfromname);
		s_fs->mountpoint	= strdup(mntbufp[i].f_mntonname);
		s_fs->blocks		= mntbufp[i].f_blocks;
		s_fs->blocksfree	= mntbufp[i].f_bfree;
		s_fs->blocksize 	= mntbufp[i].f_bsize;
		s_fs->inodes		= mntbufp[i].f_files;
		s_fs->inodesfree	= mntbufp[i].f_ffree;

		s_lstats->filesystems[fs_count] = s_fs;
		fs_count++;
	}
	s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
	if(s_lstats->filesystems == NULL) {
		return -1;
	}
	s_lstats->filesystems[fs_count] = NULL;

	return 0;
}
