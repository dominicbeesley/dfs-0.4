/*	DFS - create/read BBC Micro DFS disc images
        Copyright (C) 2005 Dominic Beesley
        
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.
                        
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
                                   
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
          
  	dom-dfs@brahms.demon.co.uk                
*/

extern unsigned int dfs_errno;

typedef unsigned char byte;

#define MAX_SECTORS	800

struct name_ent {
	char		name[7];
	char		dir;
};

struct addr_ent {
	byte		load_l;
	byte		load_h;
	byte		exec_l;
	byte		exec_h;
	byte		len_l;
	byte		len_h;
	byte		morebits;
	byte		secaddr_l;
};


struct sector0 {
	char		disc_name_start[8];
	struct name_ent	file_names[31];
};

struct sector1 {
	char		disc_name_end[4];
	byte		no_writes;
	byte		cat_count;
	byte		opt4;
	byte		seccount_l;
	struct addr_ent file_addrs[31];
};

union dfs_disc {
	byte		raw_sect[MAX_SECTORS][256];
	struct {
		struct sector0 sector0;
		struct sector1 sector1;
	} header;
};

int dfs_get_disc_info(
	union dfs_disc *dfs
,	char *disc_name		//includes NUL >13
,	unsigned int *sectors
,	unsigned int *no_writes
,	unsigned int *opt4
,	unsigned int *cat_count
);

int dfs_get_file_info(
	union dfs_disc *dfs
,	int index
,	char *file_name		//includes dir dot and NUL
				// >10chars
,	int *locked
,	unsigned long int *load
,	unsigned long int *exec
,	unsigned long int *len
,	unsigned int *sector
);

union dfs_disc *dfs_open(
	const char *filename
);

union dfs_disc *dfs_create(
	int tracks
);

int dfs_write(
	union dfs_disc *dfs
,	const char *filename
);

void dfs_close(
	union dfs_disc *dfs
);

int dfs_findfree(
	union dfs_disc *dfs
,	unsigned int sectorcount
,	unsigned int *sector
);

int dfs_addcatent(
	union dfs_disc *dfs
,	char *file_name
,	int locked
,	unsigned long int load
,	unsigned long int exec
,	unsigned long int len
,	unsigned int sec
);

int dfs_title(
	union dfs_disc *dfs
,	const char *title);

int dfs_opt4(
	union dfs_disc *dfs
,	int opt4);


const char *dfs_error_message(
	int errno
);

#define EDFS_OK		0	//No error!
#define EDFS_FILEOPEN	1	//could not open file
#define EDFS_MEMORY	2	//out of memory
#define EDFS_FORMAT	3	//format not recognised
#define EDFS_INVAL	4	//invalid parameter
#define EDFS_FILEIO	5	//IO error
#define EDFS_DISCFULL	6	//Disc full
#define EDFS_CATFULL	7	//Catalogue full
#define EDFS_DUPLICATE	8	//Duplicate filename
