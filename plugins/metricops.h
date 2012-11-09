struct s_fs {
	char			*fs;
	char			*device;
	char			*mountpoint;
	unsigned long	blocks;
	unsigned long	blocksfree;
	unsigned long	blocksize;
	unsigned long	inodes;
	unsigned long	inodesfree;
};

struct s_interface {
	char			*device;
	char			*address;
	unsigned long	rxbytes;
	unsigned long	rxpackets;
	unsigned long	txbytes;
	unsigned long	txpackets;
};

struct s_stats {
	char			*sysname;
	char			*hostname;
	char			*release;
	char			*version;
	unsigned long	time;
	unsigned long	uptime;
	float			load1;
	float			load5;
	float			load15;
	unsigned long long	totalrealmem;
	unsigned long long	availrealmem;
	unsigned long	sharedmem;
	unsigned long	buffermem;
	unsigned long	cachedmem;
	unsigned long long	totalswap;
	unsigned long long	availswap;
	unsigned long	processes;
	int				numcpus;
	unsigned long	cpusys;
	unsigned long	cpunice;
	unsigned long	cpuuser;
	unsigned long	cpuidle;
	unsigned long	pagesin;
	unsigned long	pagesout;
	unsigned long	swapsin;
	unsigned long	swapsout;
	unsigned long	contexts;
	struct s_fs		**filesystems;
	struct s_interface **interfaces;
};

int gather(struct s_stats *);

int gather(struct s_stats *);

