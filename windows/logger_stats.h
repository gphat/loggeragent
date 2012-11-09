struct s_fs {
	char			*fs;		// Type of Filesystem
	char			*device;	// Device Name
	char			*mountpoint;// Mount point
	unsigned long	blocks;		// Number of blocks
	unsigned long	blocksfree;	// Number of free blocks
	unsigned long	blocksize;  // Block size
	unsigned long	inodes;		// Number of inodes
	unsigned long	inodesfree;	// Number of free inodes
};

struct s_interface {
	char			*device;	// Interface device
	unsigned long	rxbytes;	// Recieve Bytes
	unsigned long	rxpackets;	// Recieve Packets
	unsigned long	txbytes;	// Transmit Bytes
	unsigned long	txpackets;	// Transmit Packets
};

struct s_logger_stats {
	char			*system;	// OS
	char			*hostname;	// Hostname
	char			*release;	// OS Release
	char			*version;	// OS Version
	long			time;		// Time this metric was reaped
	__int64			uptime;		// Seconds since boot
	float			load1;		// 1 load avgs
	float			load5;		// 5 load avgs
	float			load15;		// 15 load avgs
	unsigned long 	totalrealmem; // Total Real Memory
	unsigned long 	availrealmem; // Available Real Memory
	unsigned long	sharedmem;	// Shared Memory
	unsigned long	buffermem;	// Memory used by buffers
	unsigned long	cachedmem;	// Cached Memory
	unsigned long 	totalswap;	// Total Swap
	unsigned long 	availswap;	// Available Swap
	unsigned long	processes;	// Number of current processes
	int				numcpus;	// Number of CPUs
	__int64			cpusys;		// CPU time in System Mode
	unsigned long	cpunice;	// CPU time in Nice Mode
	unsigned long	cpuuser;	// CPU time in User Mode
	double			cpuidle;	// CPU time in Idle Mode
	unsigned long	pagesin;	// Pages paged in
	unsigned long	pagesout;	// Pages paged out
	unsigned long	swapsin;	// Pages paged in
	unsigned long	swapsout;	// Pages paged out
	unsigned long	contexts;	// Context Switches
	struct s_fs		**filesystems; // Filesystems
	struct s_interface **interfaces; // Interfaces
};
