#include <ctype.h>
#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <net/if.h>
#include <netinet/in.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>

#include "log.h"
#include "metricops.h"

int gather(struct s_stats *s_lstats) {
	FILE				*stream;
	char				*stat_line, *offset, *p;
	int					fs_count = 0, if_count = 0, cpu_count = 0;
	int					retval = 0, sockfd;
	struct utsname 		s_uname;
	struct mntent		*mnt;
	struct statfs		s_statfs;
	struct sysinfo		*s_sinfo;
	struct s_interface	*s_if;
	struct s_fs			*s_fs;
	struct ifreq		interface;
	struct sockaddr_in	sin;

	/* Get Information about the OS */
	if(uname(&s_uname) == -1) {
		llog(LOG_ERR, "Error in uname(2): %s", strerror(errno));
		return -1;
	}
	
	s_lstats->hostname	= strdup(s_uname.nodename);
	s_lstats->sysname	= strdup(s_uname.sysname);
	s_lstats->release	= strdup(s_uname.release);
	s_lstats->version	= strdup(s_uname.version);

	retval = (int) time(&s_lstats->time);
	if(retval == -1) {
		llog(LOG_ERR, "Error in time().");
		return -1;
	}

	/* Allocate memory for the sysinfo struct */
	s_sinfo = malloc(sizeof(struct sysinfo));
	if(s_sinfo == NULL) {
		return -1;
	}

	/* Get system information */
	retval = sysinfo(s_sinfo);
	if(retval == -1) {
		llog(LOG_ERR, "Error in sysinfo(): %s", strerror(errno));
		free(s_sinfo);
		return -1;
	}

	/* Copy the information into the s_stats struct */
	s_lstats->uptime	= 0;
	s_lstats->uptime	= (unsigned long int) s_sinfo->uptime;
	s_lstats->totalrealmem	= 0;
	s_lstats->totalrealmem	= (s_sinfo->totalram * s_sinfo->mem_unit) / 1024;
	s_lstats->availrealmem	= 0;
	s_lstats->availrealmem	= (s_sinfo->freeram * s_sinfo->mem_unit) / 1024;
	s_lstats->sharedmem	= 0;
	s_lstats->sharedmem	= (s_sinfo->sharedram * s_sinfo->mem_unit) / 1024;
	s_lstats->buffermem = 0;
	s_lstats->buffermem = (s_sinfo->bufferram * s_sinfo->mem_unit) / 1024;
	s_lstats->totalswap	= 0;
	s_lstats->totalswap	= (s_sinfo->totalswap * s_sinfo->mem_unit) / 1024;
	s_lstats->availswap	= 0;
	s_lstats->availswap	= (s_sinfo->freeswap * s_sinfo->mem_unit) / 1024;
	s_lstats->processes	= 0;
	s_lstats->processes	= s_sinfo->procs;

	free(s_sinfo);

	/* Malloc stat_line, which is the current line in the file */
	stat_line = malloc(200);
	if(stat_line == NULL) {
		llog(LOG_ERR, "Error allocating memory for statistics buffer.");
		return -1;
	}

	/* Open /proc/loadavg for load averages */
	if((stream = fopen("/proc/loadavg", "r")) == NULL) {
		llog(LOG_ERR, "Error opening /proc/loadave: %s", strerror(errno));
		free(stat_line);
		return -1;
	}

	if(fgets(stat_line, 200, stream) == NULL) {
		llog(LOG_ERR, "Error reading /proc/loadavg");
		free(stat_line);
		return -1;
	}

	s_lstats->load1		= 0.0;
	s_lstats->load5		= 0.0;
	s_lstats->load15	= 0.0;
	retval = sscanf(stat_line, "%f %f %f %*d/%*d %*d", &s_lstats->load1, &s_lstats->load5, &s_lstats->load15);
	if(retval == EOF) {
		llog(LOG_ERR, "Error in sscanf() of load averages.");
		free(stat_line);
		return -1;
	}

	retval = fclose(stream);
	if(retval != 0) {
		llog(LOG_ERR, "Error closing /proc/loadavg.");
		free(stat_line);
		return -1;
	}

	/* Open /proc/stat, this thing contains all types of useful
	 * information
	 */
	if((stream = fopen("/proc/stat", "r")) == NULL) {
		llog(LOG_ERR, "Error opening /proc/stat.");
		free(stat_line);
		return -1;
	}

	/* Read the first line of /proc/stat, this contains the number of
	 * cpu ticks spent in each mode.
	 */
	s_lstats->cpuuser	= 0;
	s_lstats->cpunice	= 0;
	s_lstats->cpusys	= 0;
	s_lstats->cpuidle	= 0;
	retval = fscanf(stream, "cpu %lu %lu %lu %lu\n", &s_lstats->cpuuser,
		&s_lstats->cpunice, &s_lstats->cpusys, &s_lstats->cpuidle);
	if(retval == EOF) {
		llog(LOG_ERR, "Error reading cpu values.");
		free(stat_line);
		return -1;
	}
	
	fgets(stat_line, 100, stream);

	/* Skip all the lines that have cpu at the beginning, as these are
	 * per-cpu counters.  Count how many there are, since we are here. 
	 */
	while(!strncmp(stat_line, "cpu", 3)) {
		fgets(stat_line, 100, stream);
		cpu_count++;
	}
	s_lstats->numcpus = cpu_count;

	/* Get the page and swap counters.
	 * sscanf() the first one, since the above loop
	 * will leave us with the line we want.
	 */
	s_lstats->pagesin	= 0;
	s_lstats->pagesout	= 0;
	s_lstats->swapsin	= 0;
	s_lstats->swapsout	= 0;
	retval = sscanf(stat_line, "page %lu %lu\n", &s_lstats->pagesin,
				&s_lstats->pagesout);
	if(retval == EOF) {
		llog(LOG_ERR, "Error reading page values.");
		free(stat_line);
		return -1;
	}
	retval = fscanf(stream, "swap %lu %lu\n", &s_lstats->swapsin,
				&s_lstats->swapsout);
	if(retval == EOF) {
		llog(LOG_ERR, "Error reading swap values.");
		free(stat_line);
		return -1;
	}

	/* Keep reading until we get the line beginning with ctxt, as we want
	 * context switches next.
	 */
	while(strncmp(stat_line, "ctxt", 4) != 0) {
		fgets(stat_line, 100, stream);
	}

	/* Actually get the context switches, use sscanf() since the
	 * above loop will position us.
	 */
	s_lstats->contexts = 0;
	retval = sscanf(stat_line, "ctxt %lu\n", &s_lstats->contexts);
	if(retval == EOF) {
		llog(LOG_ERR, "Error reading ctxt values.");
		free(stat_line);
		return -1;
	}

	retval = fclose(stream);
	if(retval != 0) {
		llog(LOG_ERR, "Error closing /proc/stat.");
		free(stat_line);
		return -1;
	}

	/* There's no better way to get cached memory info that I
	 * could find, so lets open /proc/meminfo
	 */
	if((stream = fopen("/proc/meminfo", "r")) == NULL) {
		llog(LOG_ERR, "Error opening /proc/meminfo.");
		free(stat_line);
		return -1;
	}
	/* Keep reading until we get the line beginning with Cached, as we want
	 * the amount of cached memory
	 */
	while(strncmp(stat_line, "Cached", 6) != 0) {
		fgets(stat_line, 100, stream);
	}

	s_lstats->cachedmem = 0;
	retval = sscanf(stat_line, "Cached:   %8lu kB", &s_lstats->cachedmem);
	if(retval == EOF) {
		llog(LOG_ERR, "Error reading Cached values.");
		free(stat_line);
		return -1;
	}
	
	retval = fclose(stream);
	if(retval != 0) {
		llog(LOG_ERR, "Error closing /proc/meminfo.");
		free(stat_line);
		return -1;
	}

	/* Now open /proc/net/dev */
	if((stream = fopen("/proc/net/dev", "r")) == NULL) {
		llog(LOG_ERR, "Error opening /proc/net/dev.");
		free(stat_line);
		return -1;
	}
	/* Skip the first two lines */
	fgets(stat_line, 200, stream);
	fgets(stat_line, 200, stream);

	/* Malloc enough memory for 1 interface.  The loop dynamically
	 * adds memory for them as we go.
	 */
	s_lstats->interfaces = malloc(sizeof(struct s_interface *));
	if(s_lstats->interfaces == NULL) {
		llog(LOG_ERR, "Error allocating memory for interfaces.");
		free(stat_line);
		return -1;
	}

	while(fgets(stat_line, 200, stream)) {
		if(feof(stream) != 0) {
			break;
		}
		if(if_count > 0) {
			s_lstats->interfaces = realloc(s_lstats->interfaces, (sizeof(struct s_interface *) * (if_count + 1)));
			if(s_lstats->interfaces == NULL) {
				llog(LOG_ERR, "Error reallocating memory for interfaces.");
				free(stat_line);
				return -1;
			}
		}
		/* Create a new interface struct each time through */
		s_if = malloc(sizeof(struct s_interface));
		if(s_if == NULL) {
			llog(LOG_ERR, "Error allocating memory for interface.");
			free(stat_line);
			return -1;
		}

		/* Find the : that seperates the interface name from the
		 * counters
		 */
		offset = strchr(stat_line, ':');
		/* Set the : to a 0 */
		*offset = 0;
		/* Make p point to the same place as stat_line */
		p = stat_line;
		/* Find that bloody 0 from earlier */
		while(isspace(*p)) {
			p++;
		}
		/* Now that we've moved p past all the spaces, copy it into
		 * the name member.
		 */
		s_if->device = strdup(p);
		
		/* Get the data */
		s_if->rxbytes	= 0;
		s_if->rxpackets	= 0;
		s_if->txbytes	= 0;
		s_if->txpackets	= 0;
		retval = sscanf((offset + 1), "%lu %lu %*u %*u %*u %*u %*u %*u %lu %lu %*u %*u %*u %*u %*u %*u\n",
			&s_if->rxbytes, &s_if->rxpackets,
			&s_if->txbytes, &s_if->txpackets);
		if(retval == EOF) {
			llog(LOG_ERR, "Error reading interface values.");
			free(stat_line);
			return -1;
		}
		s_lstats->interfaces[if_count] = s_if;

		/*
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd) {
			strcpy(interface.ifr_name, s_if->device);
			if(ioctl(sockfd, SIOCGIFADDR, &interface) == 0) {
				sin = *(struct sockaddr_in *) &interface.ifr_addr;
				s_if->address = strdup((char *) inet_ntoa(sin.sin_addr.s_addr));
			}
		}
		close(sockfd);
		*/
		
		if_count++;
	}
	/* Add a null to the list */
	s_lstats->interfaces = realloc(s_lstats->interfaces, (sizeof(struct s_interface *) * (if_count + 1)));
	if(s_lstats->interfaces == NULL) {
		llog(LOG_ERR, "Error reallocating memory for interfaces to add a null.");
		free(stat_line);
		return -1;
	}
	s_lstats->interfaces[if_count] = NULL;

	retval = fclose(stream);
	if(retval != 0) {
		llog(LOG_ERR, "Error closing /proc/net/dev.");
		free(stat_line);
		return -1;
	}

	/* Malloc enough memory for 1 filesystem.  The loop dynamically
	 * adds memory for them as we go.
	 */
	s_lstats->filesystems = malloc(sizeof(struct s_fs *));
	if(s_lstats->filesystems == NULL) {
		llog(LOG_ERR, "Error allocating memory for filesystems.");
		free(stat_line);
		return -1;
	}

	stream = setmntent(MOUNTED, "r");
	if(stream == NULL) {
		llog(LOG_ERR, "Error opening filepointer with setmntent().");
		free(stat_line);
		return -1;
	}
	while((mnt = getmntent(stream))) {
		if(strncmp("none", mnt->mnt_fsname, 4) &&
		  strncmp("/proc", mnt->mnt_dir, 5) &&
		  strncmp("smbfs", mnt->mnt_type, 5) &&
		  strncmp("tmpfs", mnt->mnt_type, 5) &&
		  strncmp("nfs", mnt->mnt_type, 3)) {
				if(fs_count > 0) {
					s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
					if(s_lstats->filesystems == NULL) {
						llog(LOG_ERR, "Error reallocating memory for filesystems.");
						free(stat_line);
						return -1;
					}
				}
				/* Create a new filesystem struct each time through */
				s_fs = malloc(sizeof(struct s_fs));
				if(s_fs == NULL) {
					llog(LOG_ERR, "Error allocating memory for filesystem.");
					free(stat_line);
					return -1;
				}
				retval = statfs(mnt->mnt_dir, &s_statfs);
				if(retval == -1) {
					llog(LOG_ERR, "Error calling statfs for %s.", mnt->mnt_dir);
					free(stat_line);
					return -1;
				}

				s_fs->fs		 = strdup(mnt->mnt_type);
				s_fs->device	 = strdup(mnt->mnt_fsname);
				s_fs->mountpoint = strdup(mnt->mnt_dir);
				s_fs->blocks	 = 0;
				s_fs->blocks	 = s_statfs.f_blocks;
				s_fs->blocksfree = 0;
				s_fs->blocksfree = s_statfs.f_bfree;
				s_fs->blocksize	 = 0;
				s_fs->blocksize	 = s_statfs.f_bsize;
				s_fs->inodes	 = 0;
				s_fs->inodes	 = s_statfs.f_files;
				s_fs->inodesfree = 0;
				s_fs->inodesfree = s_statfs.f_ffree;

				s_lstats->filesystems[fs_count] = s_fs;
				fs_count++;
		}
	}
	s_lstats->filesystems = realloc(s_lstats->filesystems, (sizeof(struct s_fs *) * (fs_count + 1)));
	if(s_lstats->filesystems == NULL) {
		llog(LOG_ERR, "Error reallocating memory for filesystems.");
		free(stat_line);
		return -1;
	}
	s_lstats->filesystems[fs_count] = NULL;
	retval = endmntent(stream);

	/* Clean up */
	free(stat_line);

	return 0;
}
