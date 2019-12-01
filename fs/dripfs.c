// This is the DripFS specification

// Each drive's first sector must be in the following format:

// uint32 Sector count
// A char[20] (null terminated) for the volume name
// uint32 Sector of root dir
// Rest is reserved (char[484])


// The sector of the root dir (and other folders on the drive) contains the following:

// char[50] (null terminated) for the folder name
// uint32 for the Sector where the associated Folder file sector reference table lives
// uint32 Sector of parent dir
// Reserved (char[450])


// Folder file sector reference table:

// The folder file sector reference table is simply a table of sector numbers which contain sectors where a table of sector numbers for accessing files exists

// uint32[128] Sector numbers

// The file reference table is the same thing, but for referencing file entries

// This means that you can have 16384 files in a directory

// File entries shall be structured like this:

// char[50] (null terminated) Name
// uint32 Sector of file chunk table table table (a table of tables of file chunk tables, and each file chunk table contains 128 file chunk sectors)
// uint32 Parent folder sector
// Reserved (char[450])

// This 128 table times 128 table times 128 table times 512 bytes per sector results in a max file size of 1073741824 bytes, and uses 8389120 bytes when storing the max file size

// Each sector on the drive used as a table sector must have the value 0xbbddccaa at the beginning if it is not in use, so that you can use that to find used table sectors
