// ApexKernel/Includes/Filesystem/AXFS/Entry/EntryLifecycle.h
// ApexOS -- AXFS entry creation orchestration

#ifndef APEX_FILESYSTEM_AXFS_ENTRY_LIFECYCLE_H
#define APEX_FILESYSTEM_AXFS_ENTRY_LIFECYCLE_H

// axfs_create() itself is declared in AXFS.h (part of the public
// API). This file orchestrates InodeCreation.c (build the new inode)
// and DirectoryInsertion.c (link it into the parent), with rollback
// if either step fails.

#endif // APEX_FILESYSTEM_AXFS_ENTRY_LIFECYCLE_H
