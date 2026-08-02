set -x
db2 connect to dbgen4
db2set DB2_USE_ALTERNATE_PAGE_CLEANING=ON # no need for the cleaner to wait for a threshold CHGPGS_THRESH

db2 UPDATE DATABASE CONFIGURATION FOR dbgen4 USING SELF_TUNING_MEM OFF; # disable self-tuning memory (we set it manually below)
db2 UPDATE DATABASE CONFIGURATION FOR dbgen4 USING LOGFILSIZ     32768; # log file size (normal 4096 4KB pages)
db2 UPDATE DATABASE CONFIGURATION FOR dbgen4 USING LOGBUFSZ      32768; # buffer before dumping log to disk
db2 UPDATE DATABASE CONFIGURATION FOR dbgen4 USING NUM_IOCLEANERS    8; # faster cleaner (normal 4)
db2 UPDATE DATABASE CONFIGURATION FOR dbgen4 USING DBHEAP        40000; # database heap  must be :DBHEAP > LOGBUFSZ

db2 AUTOCONFIGURE USING MEM_PERCENT 50 WORKLOAD_TYPE MIXED APPLY NONE;

db2 connect reset
db2stop force
db2start
