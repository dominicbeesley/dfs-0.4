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

#include "dfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

#define DFS_SECTORCOUNT(dfs) (unsigned int)((dfs->header.sector1.seccount_l \
		 + ((dfs->header.sector1.opt4 & 3) << 8)) )

#define DFS_CATCOUNT(dfs) (dfs->header.sector1.cat_count >> 3)
		 
#define DFS_FILE_LOCKED(dfs, index) \
		( (dfs->header.sector0.file_names[index].dir & 0x80) != 0)
		
#define DFS_EX(x) ( ((x & 0x30000) == 0x30000)? x | 0xFC0000:x)
		
#define DFS_FILE_LOAD(dfs, index) DFS_EX(( dfs->header.sector1.file_addrs[index].load_l \
		+   (dfs->header.sector1.file_addrs[index].load_h << 8) \
		+ ( (unsigned long int) (dfs->header.sector1.file_addrs[index].morebits & 0x0C) << 14) ))
		
#define DFS_FILE_LEN(dfs, index) ( dfs->header.sector1.file_addrs[index].len_l \
		+ (dfs->header.sector1.file_addrs[index].len_h << 8) \
		+ ( (unsigned long int) (dfs->header.sector1.file_addrs[index].morebits & 0x30) << 12) )

#define DFS_FILE_EXEC(dfs, index) DFS_EX(( dfs->header.sector1.file_addrs[index].exec_l \
		+ (dfs->header.sector1.file_addrs[index].exec_h << 8) \
		+ ( ((unsigned long int) dfs->header.sector1.file_addrs[index].morebits & 0xC0) << 10) ))

#define DFS_FILE_SECTOR(dfs, index) ( dfs->header.sector1.file_addrs[index].secaddr_l \
		+ ( (dfs->header.sector1.file_addrs[index].morebits & 0x03 ) << 8) )
		
		
		
unsigned int dfs_errno;

int dfs_get_disc_info(
	union dfs_disc *dfs
,	char *disc_name		//includes NUL >13
,	unsigned int *sectors
,	unsigned int *no_writes
,	unsigned int *opt4
,	unsigned int *cat_count
) {
	int i;
	
	memcpy(disc_name, dfs->header.sector0.disc_name_start, 8);
	memcpy(disc_name+8, dfs->header.sector1.disc_name_end, 4);
	disc_name[12] = 0;
	for (i=0; i<13; i++) {
		if (disc_name[i] <= 32)
			disc_name[i] = 0;
	}
	*sectors = DFS_SECTORCOUNT(dfs);
	*no_writes = dfs->header.sector1.no_writes;
	*opt4 = (dfs->header.sector1.opt4 & 0x30) >> 4;
	*cat_count = DFS_CATCOUNT(dfs);
	
	return EDFS_OK;
}

union dfs_disc *dfs_open(const char *filename) {
	
	FILE *fp = NULL;
	union dfs_disc *ret = NULL; 
	unsigned int sectors = 0;
	 
	fp = fopen(filename, "rb");
	if (!fp) {
		dfs_errno = EDFS_FILEOPEN; 
		goto err;
	}
	
	ret = (union dfs_disc *)malloc(sizeof(union dfs_disc));
	if (!ret) {
		dfs_errno = EDFS_MEMORY;
		goto err;
	}
	
	sectors = fread(ret, 256, MAX_SECTORS, fp);
	
	/*if (sectors != DFS_SECTORCOUNT(ret) ) {
		fprintf(stderr, "sector count mismatch %d <> %d\n"
		, sectors
		, DFS_SECTORCOUNT(ret)
		);
		dfs_errno = EDFS_FORMAT;
		goto err;
	}*/
	sectors = DFS_SECTORCOUNT(ret);
/*	if (sectors != 400 && sectors != 800) {
		fprintf(stderr, "unrecognised format %d sectors\n", sectors);
		dfs_errno = EDFS_FORMAT;
		goto err;
	}
*/	
	if (DFS_CATCOUNT(ret) > 31) {
		fprintf(stderr, "too many files in catalog %d\n", DFS_CATCOUNT(ret));
		dfs_errno = EDFS_FORMAT;
		goto err;
	}
	
	fclose(fp);
	
	return ret;
err:
	if (fp) fclose(fp);
	if (ret) free(ret);
	return NULL;
}

const char *dfs_error_message(
	int errno
) {
	switch (errno) {
		case EDFS_FILEOPEN: return "could not open file";
		case EDFS_MEMORY: return "out of memory";
		case EDFS_FORMAT: return "unrecognised format or corrupt disc";
		case EDFS_INVAL: return "invalid parameter";
		case EDFS_FILEIO: return "IO error";
		case EDFS_DISCFULL: return "disc full";
		case EDFS_CATFULL: return "cat full (max. 31 files)";
		case EDFS_DUPLICATE: return "duplicate filename";
		default: return "unknown error";
	}
}

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
) {

	int i;

	if (index < 0 || index >= DFS_CATCOUNT(dfs)) {
		dfs_errno = EDFS_INVAL;
		return -1;
	}
	
	file_name[0] = dfs->header.sector0.file_names[index].dir & 0x7F;
	file_name[1] = '.';
	memcpy(file_name+2, dfs->header.sector0.file_names[index].name, 7);
	file_name[9] = 0;
	for (i=0; i<9; i++) {
		if (file_name[i] <= 32)
			file_name[i] = 0;
	}
	
	*locked = DFS_FILE_LOCKED(dfs, index);
	*load = DFS_FILE_LOAD(dfs, index);
	*exec = DFS_FILE_EXEC(dfs, index);
	*len = DFS_FILE_LEN(dfs, index);
	*sector = DFS_FILE_SECTOR(dfs, index);	
	
	return 0;
}

union dfs_disc *dfs_create(
	int tracks
) {
	
	union dfs_disc *ret;
	
	ret = (union dfs_disc *)calloc(tracks * 10, 256);
	if (!ret) {
		dfs_errno = EDFS_MEMORY;
		return NULL;
	}
	
	memset(ret->raw_sect[0], 32, 256);
	
	memset(ret->header.sector1.disc_name_end, 32, 4);
	
	ret->header.sector1.no_writes = 0;
	ret->header.sector1.cat_count = 0;
	ret->header.sector1.opt4 = (10 * tracks) >> 8;
	ret->header.sector1.seccount_l = (10 * tracks) & 0xFF;
	
	return ret;
}

void dfs_close(
	union dfs_disc *dfs
) {
	free(dfs);
}


int dfs_write(
	union dfs_disc *dfs
,	const char *filename
) {
	unsigned int sectors = DFS_SECTORCOUNT(dfs);

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		dfs_errno = EDFS_FILEOPEN;
		return -1;
	}
	
	if (fwrite(dfs, 256, sectors, fp) != sectors) {
		fclose(fp);
		dfs_errno = EDFS_FILEIO;
		return -1;
	}
	
	fclose(fp);
	
	return 0;
}

struct freearea {
	unsigned int start;
	unsigned int end;
	struct freearea *next;
};

struct freearea * mkfa(
	unsigned int start
,	unsigned int end
,	struct freearea *next
) {	
	struct freearea *tmp = (struct freearea *)malloc(sizeof(struct freearea)); 
	
	if (!tmp) return NULL;
	tmp->start = start; 
	tmp->end = end; 
	tmp->next = next; 
	return tmp; 
}

#define DELFA(fe) \
	{	if (freemap == fe) { \
			freemap = freemap -> next; \
			free(fe); \
		} else { \
			struct freearea *tmp = freemap; \
			while (tmp->next) { \
				if (tmp->next == fe) { \
					tmp->next = tmp->next->next; \
					break; \
				} \
				tmp = tmp->next; \
			} \
			free(fe); \
		} \
	} \

#define LENFE(fe) (fe->end - fe->start + 1)
	
int dfs_findfree(
	union dfs_disc *dfs
,	unsigned int sectorcount
,	unsigned int *sector
) {
	int i, ret = 0;
	unsigned int start, end, smallest;
	struct freearea *freemap, *fe, *fenext;
	
	/* construct an ordered (by start sector) list of free areas */
	
	//start with whole disc free - note 0,1 used by catalogue
	 
	freemap = mkfa(2, DFS_SECTORCOUNT(dfs)-1, NULL);
	if (!freemap) {
		dfs_errno = EDFS_MEMORY;
		ret = -1;
		goto out;
	}
	
	for (i=0; i<DFS_CATCOUNT(dfs); i++) {
		start = DFS_FILE_SECTOR(dfs, i);
		end = start + ((DFS_FILE_LEN(dfs, i) - 1) / 256);
				
		fe = freemap;
		while (fe) {
			if (start <= fe->start && end >= fe->end) {
				/* file surrounds free area - remove it */
				fenext = fe->next;
				DELFA(fe);
				fe = fenext;
				continue;
			} else if (start == fe->start && end < fe->end) {
				/* some free left at end adjust start*/
				fe->start = end + 1;
				fe = fe->next;
			} else if (start >= fe->start && end == fe->end) {
				/* some free left at start adjust end */
				fe->end = start - 1;
				fe = fe->next;
			} else if (start >= fe->start && end <= fe->end) {
				/* file in middle of area, make a new one for the free
				   space _after_ and adjust current for space _before_
				   the file */
				fenext = mkfa(end + 1, fe->end, fe->next);
				if (!fenext) {
					dfs_errno = EDFS_MEMORY;
					ret = -1;
					goto out;
				}
				fe->end = start - 1;
				fe = fe->next = fenext;
			} else {
				fe = fe->next;
			}	
		}
		
	}
	
	
	/* find smallest free area that fits our purpose */
	smallest = UINT_MAX;
	fe = freemap;
	*sector = 0;
	while (fe != NULL) {
		if ( LENFE(fe) >= sectorcount && LENFE(fe) < smallest ) {
			smallest = LENFE(fe);
			*sector = fe->start;
		}
		fe = fe->next;
	}
	
	if (*sector == 0) {
		ret = -1;
		dfs_errno = EDFS_DISCFULL;
	}
	
	
out:
	while (freemap) {
		fe = freemap->next;
		free(freemap);
		freemap = fe;
	}
	return ret;
}

static void dfs_getname(
	union dfs_disc *dfs
,	int index
,	char file_name[9]// 9 chars , no nul but padded with spaces
) {
	file_name[0] = dfs->header.sector0.file_names[index].dir & 0x7F;
	file_name[1] = '.';
	memcpy(file_name+2, dfs->header.sector0.file_names[index].name, 7);
	return;
}
	
void dfs_swapbuf(byte *a, byte *b, int len) {
	int i;
	byte tmp;
	
	for (i = 0; i<len; i++) {
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
	}
}

void dfs_swapent(union dfs_disc *dfs, int a, int b) {
	if (a==b) return;
	
	dfs_swapbuf(	(byte *)&dfs->header.sector0.file_names[a]
	,		(byte *)&dfs->header.sector0.file_names[b]
	,		sizeof(dfs->header.sector0.file_names[a])
	);
	
	dfs_swapbuf(	(byte *)&dfs->header.sector1.file_addrs[a]
	,		(byte *)&dfs->header.sector1.file_addrs[b]
	,		sizeof(dfs->header.sector1.file_addrs[a])
	);
}
	

void dfs_sortcat(union dfs_disc *dfs) {
	
	int swap = 1, i, ct;
	
	ct = DFS_CATCOUNT(dfs);
	
	while (swap) { 
		i = 0;
		swap = 0;
		while (i < ct - 1) {
			if (DFS_FILE_SECTOR(dfs, i) < DFS_FILE_SECTOR(dfs, i+1)) {
				dfs_swapent(dfs, i, i + 1);
				swap = 1;
			}	
			i++;
		}
	}
}

int dfs_addcatent(
	union dfs_disc *dfs
,	char *file_name
,	int locked
,	unsigned long int load
,	unsigned long int exec
,	unsigned long int len
,	unsigned int sec
) {
	char realname[9], *p, cmpname[9];
	int i;
	
	if (DFS_CATCOUNT(dfs)>=31) {
		dfs_errno = EDFS_CATFULL;
		return -1;
	}
	
	if (file_name[0]!=0 && file_name[1]=='.') {
		realname[0] = toupper(file_name[0]);
		p = (file_name + 2);
	} else {
		realname[0] = '$';
		p = file_name;
	}
	
	realname[1] = '.';
	
	for (i = 0; i < 7; i++) {
		if (*p) {
			realname[2+i] = toupper(*p);
			p++;
		} else {
			realname[2+i] = ' ';
		}
	}

	for (i = 0; i < DFS_CATCOUNT(dfs); i++) {
		dfs_getname(dfs, i, cmpname);
		if (!strncmp(realname, cmpname, 9)) {
			dfs_errno = EDFS_DUPLICATE;
			fprintf(stderr, "duplicate filename: %.9s\n", realname);
			return -1;
		}
	}	
	
	i = DFS_CATCOUNT(dfs);
	
	dfs->header.sector0.file_names[i].dir = realname[0] 
		| ((locked)?0x80:0);
	memcpy(dfs->header.sector0.file_names[i].name, realname + 2, 7);
	
	struct addr_ent *ae = &dfs->header.sector1.file_addrs[i];
	
	ae->load_l = load & 0xFF;
	ae->load_h = (load & 0xFF00) >> 8;
	
	ae->exec_l = exec & 0xFF;
	ae->exec_h = (exec & 0xFF00) >> 8;
	
	ae->len_l = len & 0xFF;
	ae->len_h = (len & 0xFF00) >> 8;
	
	ae->morebits = ((sec & 0x0300) >> 8)
		     | ((load & 0x30000) >> 14 )
		     | ((len & 0x30000) >> 12 )
		     | ((exec & 0x30000) >> 10 );
	
	ae->secaddr_l = sec;
	
	dfs->header.sector1.no_writes++;
	dfs->header.sector1.cat_count+=8;
	
	/* now sort the catalogue into reverse sector order */
	dfs_sortcat(dfs);
	
	return 0;
}

int dfs_opt4(union dfs_disc *dfs, int opt4)
{
	dfs->header.sector1.opt4 &= 0x03;
	dfs->header.sector1.opt4 |= (opt4 & 0x3) << 4;
	return 0;
}

int dfs_title(union dfs_disc *dfs, const char *title)
{
	char buf[12];
	snprintf(buf, 12, "%-12.12s", title);
	memcpy(dfs->header.sector0.disc_name_start, buf, 8);
	memcpy(dfs->header.sector1.disc_name_end, buf + 8, 4);
	return 0;
}