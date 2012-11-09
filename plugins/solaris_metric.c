#include <ctype.h>
#include <kstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/mnttab.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>

#include "log.h"
#include "metricops.h"

static ulong_t *get_named_member(kstat_t *, char *);

int gather(struct s_stats *s_lstats) {
	FILE				*stream;
	char				*ifkey, *stat_line, *offset;
	int					fs_count = 0, if_count = 0;
	struct mnttab		s_mnt;
	struct statvfs		s_statvfs;
	struct s_interface	*s_if;
	struct s_fs			*s_fs;
	struct utsname		s_uname;
	ulong_t				clk_intr, *rxpackets, *txpackets, *rxbytes, *txbytes;
	unsigned long		hz, page_size;
	unsigned long long	avpages, totpages, swapavail, swapused;
	kstat_ctl_t			*kc;
	kstat_t				*kp;
	kstat_named_t		*kn;
	cpu_stat_t			*statp;
	cpu_sysinfo_t		*sysinfop;
	cpu_vminfo_t		*vminfop;

	/* Get Information about the OS */
	uname(&s_uname);
	s_lstats->hostname	= strdup(s_uname.nodename);
	s_lstats->sysname	= strdup(s_uname.sysname);
	s_lstats->release	= strdup(s_uname.release);
	s_lstats->version	= strdup(s_uname.version);

	s_lstats->contexts	= 0;
	s_lstats->totalswap	= 0;
	s_lstats->availswap	= 0;
	s_lstats->totalrealmem	= 0;
	s_lstats->availrealmem	= 0;

	time(&s_lstats->time);

	/* Get the clock ticks/sec */
	hz = sysconf(_SC_CLK_TCK);
	/* Get the page size */
	page_size = sysconf(_SC_PAGE_SIZE);
	/* Total and Available Physical Pages */
	totpages = sysconf(_SC_PHYS_PAGES);
	avpages = sysconf(_SC_AVPHYS_PAGES);

	if(!(kc = kstat_open())) {
		llog(LOG_ERR, "Failed to open kstat.");
		return -1;
	}

	/* Get the load averages */
	if(!(kp = kstat_lookup(kc, "unix", 0, "system_misc"))) {
		llog(LOG_ERR, "Failed to lookup system_misc");
		return -1;
	}
	if(kstat_read(kc, kp, 0) < 0) {
		llog(LOG_ERR, "Failed to read kstat.");
		return -1;
	}
	/* Get the number of clock interrupts */
	kn = (kstat_named_t *)kstat_data_lookup(kp, "clk_intr");
	clk_intr = kn->value.ul;
	s_lstats->uptime = 0;
	s_lstats->uptime = clk_intr / hz;
	kn = (kstat_named_t *)kstat_data_lookup(kp, "avenrun_1min");
	s_lstats->load1 = 0.0;
	s_lstats->load1 = ((double) kn->value.ul) / FSCALE;
	kn = (kstat_named_t *)kstat_data_lookup(kp, "avenrun_5min");
	s_lstats->load5 = 0.0;
	s_lstats->load5 = ((double) kn->value.ul) / FSCALE;
	kn = (kstat_named_t *)kstat_data_lookup(kp, "avenrun_15min");
	s_lstats->load15 = 0.0;
	s_lstats->load15 = ((double) kn->value.ul) / FSCALE;

	/* Number of processes */
	kn = (kstat_named_t *)kstat_data_lookup(kp, "nproc");
	s_lstats->processes = ((double) kn->value.ul);

	/* Copy the information into the s_stats struct */
	s_lstats->totalrealmem	= (totpages * page_size) / 1024;
	s_lstats->availrealmem	= (avpages * page_size) / 1024;
	/* Set these to zero, else they report hugeass numbers */
	s_lstats->sharedmem	= 0;
	s_lstats->buffermem 	= 0;
	s_lstats->cachedmem 	= 0;

	stat_line = malloc(200);
	if(stat_line == NULL) {
		return -1;
	}
	memset(stat_line, 0, 200);

	stream = popen("swap -s", "r");
	fgets(stat_line, 200, stream);
	pclose(stream);
	offset = strstr(stat_line, "reserved = ");
	sscanf(offset + 11, "%lluk used, %lluk available", &swapused, &swapavail);
	s_lstats->totalswap		= swapused + swapavail;
	s_lstats->availswap		= swapavail;
	
	free(stat_line);

	s_lstats->cpuidle	= 0;
	s_lstats->cpuuser	= 0;
	s_lstats->cpusys	= 0;
	s_lstats->pagesin	= 0;
	s_lstats->pagesout	= 0;
	s_lstats->swapsin	= 0;
	s_lstats->swapsout	= 0;
	s_lstats->numcpus	= 0;
	
	for(kp = kc->kc_chain; kp; kp = kp->ks_next) {
		if(strcmp(kp->ks_module, "cpu_stat") != 0) {
			continue;
		}

		kstat_read(kc, kp, 0);

        /* For future reference, wait times can be gotten from
		 * &statp->cpu_syswait
         */
		statp = (cpu_stat_t *)kp->ks_data;
		sysinfop = &statp->cpu_sysinfo;
		vminfop  = &statp->cpu_vminfo;

		s_lstats->cpuidle	 += sysinfop->cpu[0];
		s_lstats->cpuuser	 += sysinfop->cpu[1];
		s_lstats->cpusys	 += sysinfop->cpu[2];
		s_lstats->pagesin	 += vminfop->pgpgin;
		s_lstats->pagesout	 += vminfop->pgpgout;
		s_lstats->swapsin	 += vminfop->pgswapin;
		s_lstats->swapsout	 += vminfop->pgswapout;

		s_lstats->numcpus++;
	}
	s_lstats->cpunice	 = 0;

	/* Malloc enough memory for 1 interface.  The loop dynamically
	 * adds memory for them as we go.
	 */
	s_lstats->interfaces = malloc(sizeof(struct t_interface *));
	if(s_lstats->interfaces == NULL) {
		return -1;
	}

	ifkey = malloc(sizeof(char) * 5);
	if(ifkey == NULL) {
		return -1;
	}
	for(kp = kc->kc_chain; kp; kp = kp->ks_next) {
		if(kp->ks_type != KSTAT_TYPE_NAMED) {
			continue;
		}
		if(strcmp(kp->ks_module, "hme") != 0) {
			if(strcmp(kp->ks_module, "ge") != 0) {
				if(strcmp(kp->ks_module, "eri") != 0) {
					if(strcmp(kp->ks_module, "ce") != 0) {
						if(strcmp(kp->ks_module, "qfe") != 0) {
							continue;
						}
					}
				}
			}
		}
		if(if_count > 0) {
			s_lstats->interfaces = realloc(s_lstats->interfaces, sizeof(struct interface *) * (if_count + 1));
			if(s_lstats->interfaces == NULL) {
				return -1;
			}
		}
		/* Create a new interface struct each time through */
		s_if = malloc(sizeof(struct s_interface));
		if(s_if == NULL) {
			return -1;
		}

		sprintf(ifkey, "%s%d", kp->ks_module, kp->ks_instance);
		if(!(kp = kstat_lookup(kc, kp->ks_module, kp->ks_instance, ifkey))) {
			llog(LOG_ERR, "Failed to lookup interface");
			return -1;
		}
		kstat_read(kc, kp, 0);

		s_if->device = strdup(kp->ks_name);

		rxbytes = get_named_member(kp, "rbytes");
		s_if->rxbytes = 0;
		s_if->rxbytes = (unsigned long) *rxbytes; 

		txbytes = get_named_member(kp, "obytes");
		s_if->txbytes = 0;
		s_if->txbytes = (unsigned long) *txbytes;

		rxpackets = get_named_member(kp, "ipackets");
		s_if->rxpackets = 0;
		s_if->rxpackets = (unsigned long) *rxpackets;

		txpackets = get_named_member(kp, "opackets");
		s_if->txpackets = 0;
		s_if->txpackets = (unsigned long) *txpackets; 

		s_lstats->interfaces[if_count] = s_if;

		if_count++;
	}

	/* Add a null to the list */
	s_lstats->interfaces = realloc(s_lstats->interfaces, sizeof(struct interface *) * (if_count + 1));
	if(s_lstats->interfaces == NULL) {
		return -1;
	}
	s_lstats->interfaces[if_count] = NULL;

	/* Malloc enough memory for 1 filesystem.  The loop dynamically
	 * adds memory for them as we go.
	 */
	s_lstats->filesystems = malloc(sizeof(struct s_fs *));
	if(s_lstats->filesystems == NULL) {
		return -1;
	}

	if((stream = fopen(MNTTAB, "r")) == NULL) {
		return -1;
	}
	while(getmntent(stream, &s_mnt) == 0) {
		/* There are a ton of filesystem types that
		 * we don't care about, so ignore them.
		 */
		if(strncmp("proc", s_mnt.mnt_fstype, 4) &&
		  strncmp("mntfs", s_mnt.mnt_fstype, 5) &&
		  strncmp("autofs", s_mnt.mnt_fstype, 5) &&
		  strncmp("nfs", s_mnt.mnt_fstype, 5) &&
		  strncmp("fd", s_mnt.mnt_fstype, 5) &&
		  strncmp("hsfs", s_mnt.mnt_fstype, 4) &&
		  strncmp("tmpfs", s_mnt.mnt_fstype, 3)) {
			if(fs_count > 0) {
				s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
				if(s_lstats->filesystems == NULL) {
					return -1;
				}
			}
			/* Create a new interface struct each time through */
			s_fs = malloc(sizeof(struct s_fs));
			if(s_fs == NULL) {
				return -1;
			}
			statvfs(s_mnt.mnt_mountp, &s_statvfs);
			s_fs->fs			= strdup(s_mnt.mnt_fstype);
			s_fs->device		= strdup(s_mnt.mnt_special);
			s_fs->mountpoint	= strdup(s_mnt.mnt_mountp);
			s_fs->blocks		= 0;
			s_fs->blocks 		= s_statvfs.f_blocks;
			s_fs->blocksfree	= 0;
			s_fs->blocksfree 	= s_statvfs.f_bavail;
			s_fs->blocksize		= 0;
			s_fs->blocksize 	= s_statvfs.f_frsize;
			s_fs->inodes		= 0;
			s_fs->inodes 		= s_statvfs.f_files;
			s_fs->inodesfree	= 0;
			s_fs->inodesfree 	= s_statvfs.f_ffree;

			s_lstats->filesystems[fs_count] = s_fs;
			fs_count++;
		}
	}
	fclose(stream);

	s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
	if(s_lstats->filesystems == NULL) {
		return -1;
	}
	s_lstats->filesystems[fs_count] = NULL;

	/* Clean up */
	free(stat_line);

	kstat_close(kc);

	return 0;
}

static ulong_t *get_named_member(kstat_t *kp, char *name) {
	kstat_named_t *kn;

	kn = (kstat_named_t *) kstat_data_lookup(kp, name);
	if(kn == 0) {
		llog(LOG_ERR, "Can not find member: %s", name);
		return (ulong_t *) -1;
	}

	return &kn->value.ul;
}
