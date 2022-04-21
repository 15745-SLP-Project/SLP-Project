#define DECLARE_CCTK_PARAMETERS \
CCTK_INT ghost_size&&\
CCTK_INT ghost_size_x&&\
CCTK_INT ghost_size_y&&\
CCTK_INT ghost_size_z&&\
CCTK_INT global_nsize&&\
CCTK_INT global_nx&&\
CCTK_INT global_ny&&\
CCTK_INT global_nz&&\
CCTK_INT periodic&&\
CCTK_INT periodic_x&&\
CCTK_INT periodic_y&&\
CCTK_INT periodic_z&&\
COMMON /driverrest/ghost_size,ghost_size_x,ghost_size_y,ghost_size_z,global_nsize,global_nx,global_ny,global_nz,periodic,periodic_x,periodic_y,periodic_z&&\
CCTK_STRING  info&&\
CCTK_STRING  initialize_memory&&\
CCTK_STRING  partition&&\
CCTK_STRING  partition_1d_x&&\
CCTK_STRING  partition_2d_x&&\
CCTK_STRING  partition_2d_y&&\
CCTK_STRING  partition_3d_x&&\
CCTK_STRING  partition_3d_y&&\
CCTK_STRING  partition_3d_z&&\
CCTK_STRING  processor_topology&&\
CCTK_STRING  storage_verbose&&\
CCTK_INT cacheline_mult&&\
CCTK_INT enable_all_storage&&\
CCTK_INT local_nsize&&\
CCTK_INT local_nx&&\
CCTK_INT local_ny&&\
CCTK_INT local_nz&&\
CCTK_INT overloadabort&&\
CCTK_INT overloadarraygroupsizeb&&\
CCTK_INT overloadbarrier&&\
CCTK_INT overloaddisablegroupcomm&&\
CCTK_INT overloaddisablegroupstorage&&\
CCTK_INT overloadenablegroupcomm&&\
CCTK_INT overloadenablegroupstorage&&\
CCTK_INT overloadevolve&&\
CCTK_INT overloadexit&&\
CCTK_INT overloadgroupdynamicdata&&\
CCTK_INT overloadmyproc&&\
CCTK_INT overloadnprocs&&\
CCTK_INT overloadparallelinit&&\
CCTK_INT overloadquerygroupstorageb&&\
CCTK_INT overloadsyncgroup&&\
CCTK_INT padding_active&&\
CCTK_INT padding_address_spacing&&\
CCTK_INT padding_cacheline_bits&&\
CCTK_INT padding_size&&\
CCTK_INT processor_topology_1d_x&&\
CCTK_INT processor_topology_2d_x&&\
CCTK_INT processor_topology_2d_y&&\
CCTK_INT processor_topology_3d_x&&\
CCTK_INT processor_topology_3d_y&&\
CCTK_INT processor_topology_3d_z&&\
CCTK_INT storage_report_every&&\
CCTK_INT timer_output&&\
COMMON /PUGHpriv/info,initialize_memory,partition,partition_1d_x,partition_2d_x,partition_2d_y,partition_3d_x,partition_3d_y,partition_3d_z,processor_topology,storage_verbose,cacheline_mult,enable_all_storage,local_nsize,local_nx,local_ny,local_nz,overloadabort,overloadarraygroupsizeb,overloadbarrier,overloaddisablegroupcomm,overloaddisablegroupstorage,overloadenablegroupcomm,overloadenablegroupstorage,overloadevolve,overloadexit,overloadgroupdynamicdata,overloadmyproc,overloadnprocs,overloadparallelinit,overloadquerygroupstorageb,overloadsyncgroup,padding_active,padding_address_spacing,padding_cacheline_bits,padding_size,processor_topology_1d_x,processor_topology_2d_x,processor_topology_2d_y,processor_topology_3d_x,processor_topology_3d_y,processor_topology_3d_z,storage_report_every,timer_output&&\
CCTK_REAL  cctk_final_time&&\
CCTK_REAL  cctk_initial_time&&\
CCTK_STRING  terminate&&\
CCTK_INT cctk_itlast&&\
CCTK_INT terminate_next&&\
COMMON /CACTUSrest/cctk_final_time,cctk_initial_time,terminate,cctk_itlast,terminate_next&&\


