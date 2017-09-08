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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "dfs.h"
#include "config.h"

#define MAXFILENAME 512

#define hex(i) (((i % 16)<10) \
		?'0' + i % 16 \
		:'A' + i % 16 )

#define DIRSEP '/'
#define EXTSEP '.'
#define EXTSEP2 "."	
		
#ifdef __riscos__
#ifndef __TARGET_SCL__

/* turn off riscosify stuff */

#include <unixlib/local.h>
int __riscosify_control = __RISCOSIFY_NO_PROCESS;

#endif

#undef DIRSEP
#undef EXTSEP
#undef EXTSEP2
#define DIRSEP '.'
#define EXTSEP '/'
#define EXTSEP2 "/"
#endif


unsigned int crc(const byte *buf, unsigned long int len) {
	unsigned int crc=0;
     	byte b;
     	int k;
     
     	while (len)
      	{
       		b = *buf++;
       		crc ^= (b << 8);
       		for(k=0; k<8; k++) {
          		if (crc & 32768) 
				crc=(((crc ^ 0x0810) & 32767) << 1)+1;
          		else
				crc = crc << 1;
         	}
		len--;
      	}
      	return crc;
}



//filename must be complete with $.XXX or whatever

int matchwildcardfilename(const char *fp, const char *wp) {


again:
	
	if ( *fp == '\0' && *wp=='\0') {
		return 0;
	}
	
	if ( *wp == '*' ) {
		wp ++;
		if (!*wp) return 0;
		while (*fp) {
			if (!matchwildcardfilename(fp, wp))
				return 0;
			fp ++;
		}
		return 1;
	}

	if ( toupper(*fp) != toupper(*wp) ) return 1;
	
	fp++;
	wp++;
	
	goto again;
}

int matchwildcard(const char *filename, const char *wildcard) {

	const char *fp=filename;
	const char *wp=wildcard;
	int ret = 0;
	
	
	char dir;
	if (wp[0] == '.') {
		dir = '$';
		wp++;
	} else if (wp[0] && wp[1] == '.') {
		dir = wp[0];
		wp += 2;
	} else 
		dir = '$';
		
	if (!fp[0] || fp[1] != '.') {
		return -1;
	}
	
	
	if (dir != '*') {
		ret = toupper(dir) - toupper(fp[0]);
	}
	fp += 2;
	if (ret) return ret;
			
	return matchwildcardfilename(fp, wp);
		
}

#define MAXFILE 100


void bbcfilename2local(const char *bbcfile, char *localfile, int len) {

	const char *ext=NULL;
	len --; //leave room for nul char

	if (bbcfile[1] == '.') {
	
		if (bbcfile[0] != '$') {
			ext = bbcfile;
		}
		
		bbcfile+=2;
	}

	while (*bbcfile) {
		if (!isalnum(*bbcfile)) {
			if (len<3) goto done;
			*localfile++ = '_';
			*localfile++ = hex(*bbcfile / 16);
			*localfile++ = hex(*bbcfile % 16);
			bbcfile++;
			len-=3;
		} else {
			if (len<1) goto done;
			*localfile++ = *bbcfile++;
			len--;
		}
	}
	
	if (ext && len) {
		*localfile++ = EXTSEP;
		len--;
		
		if (!len) goto done;
		
		if (!isalnum(*ext)) {
			if (len<3) goto done;
			*localfile++ = '_';
			*localfile++ = hex(*ext / 16);
			*localfile++ = hex(*ext % 16);
			len-=3;
		} else {
			if (len<1) goto done;
			*localfile++ = *ext;
			len--;
		}
	}
	
done:
	
	*localfile='\0';
}

void makefilename(char *buf, const char *dir, const char *bbcfil, const char *ext) {

	char fil[MAXFILE];
	
	bbcfilename2local(bbcfil, fil, MAXFILE);

	if (dir) {
		if (ext) {
			snprintf(buf, MAXFILENAME, "%s%c%s%c%s", dir, DIRSEP, fil, EXTSEP, ext);
		} else {
			snprintf(buf, MAXFILENAME, "%s%c%s", dir, DIRSEP, fil);
		}
	} else {
		if (ext) {
			snprintf(buf, MAXFILENAME, "%s%c%s", fil, EXTSEP, ext);
		} else {
			snprintf(buf, MAXFILENAME, "%s", fil);
		}
	}
	
}

int doform(const char *filename
,	int tracks) {

	int ret = 0;

	union dfs_disc *dfs= dfs_create(tracks);
	if (!dfs) {
		fprintf(stderr, "Could not create disc : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}
	
	if (dfs_write(dfs, filename)) {
		fprintf(stderr, "Could not write disc : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}
	
out:
	if (dfs)
		dfs_close(dfs);
	return ret;

}

int doadd(const char *imagefilename
,	unsigned long int load
,	unsigned long int exec
,	int locked
,	const char *bbcfile_in
,	const char *filename_in
) {
	int ret = 0;
	char *filename = NULL;
	char *inffilename = NULL;
	char *bbcfilename = NULL;
	FILE *fin = NULL;
	union dfs_disc *dfs = NULL;
	unsigned int sector;
	unsigned long int length = 0;
	
	
	if (bbcfile_in) {
		bbcfilename = strdup(bbcfile_in);
		if (!bbcfilename) {
			ret = 1;
			goto out;
		}
	}
	
	if (strlen(filename_in)>4) {
		const char *p = strchr(filename_in, '\0');
		
		p -= 4;
		
		if (p[0] == EXTSEP 
		&& toupper(p[1]) == 'I'
		&& toupper(p[2]) == 'N'
		&& toupper(p[3]) == 'F'
		) {
			inffilename = strdup(filename_in);
			if (!inffilename) {
				ret = 1; 
				goto out;
			}
			filename = (char *)malloc(p - filename_in + 1);
			if (!filename) {
				ret = 1;
				goto out;
			}
			memcpy(filename, filename_in, p - filename_in);
			filename[ p - filename_in] = '\0';
		}	
	}
	
	if (!filename) {
		filename = strdup(filename_in);
		if (!filename) {
			ret = 1;
			goto out;
		}
		inffilename = (char *) malloc(strlen(filename) + 5);
		if (!inffilename) {
			ret = 1;
			goto out;
		}
		strcpy(inffilename, filename);
		char *p = strchr(inffilename, '\0');
		*p++ = EXTSEP;
		*p++ = 'i';
		*p++ = 'n';
		*p++ = 'f';
		*p++ = '\0';
	}
		
	/* don't make a fuss if no inffile */
	FILE *finf = fopen(inffilename, "r");
	if (finf) {
			
		char buf[256], *p, *q;
		fgets(buf, 256, finf);
				
		fclose(finf);
		finf = NULL;
				
		p = buf;
		
		while (*p == ' ') p++;
		q = p;
		while (*q && *q != ' ') q++;
		
		if (!bbcfilename) {
			bbcfilename = (char *)malloc( (q - p) + 1);
			if (!bbcfilename) {
				ret = 1;
				goto out;
			}			
			memcpy(bbcfilename, p, q - p);
			bbcfilename[ q - p ] = '\0';
		}
		
		
		p = q;
		while (*p == ' ') p++;
		q = p;
		while (*q && *q != ' ') q++;

		if (load == 0xF000000) {
			load = strtoul(p, NULL, 16);
		}
					
		p = q;
		while (*p == ' ') p++;
		q = p;
		while (*q && *q != ' ') q++;

		if (exec == 0xF000000) {
			exec = strtoul(p, NULL, 16);
		}

		/* skip length */
		p = q;
		while (*p == ' ') p++;
		q = p;
		while (*q && *q != ' ') q++;

		p = q;
		while (*p == ' ') p++;

		if (locked == -1) {
			if (toupper(*p) == 'L') {
				locked = 1;
			} else {
				locked = 0;
			}
		}
				
	}

	if (load == 0xF000000) load = 0x0;
	if (exec == 0xF000000) exec = 0xFFFFFF;
	if (locked == -1) locked = 0;
	if (!bbcfilename)
		bbcfilename = strdup(filename);
	if (!bbcfilename) {
		ret = 1;
		goto out;
	}
	fin = fopen(filename, "r");
	if (!fin) {
		fprintf(stderr, "Cannot open \"%s\" for input\n", filename);
		ret = 2;
		goto out;
	}
	
	fseek(fin, 0L, SEEK_END);
	length = ftell(fin);
	fseek(fin, 0L, SEEK_SET);
	
	dfs = dfs_open(imagefilename);
	if (!dfs) {
		fprintf(stderr, "Could not open disc \"%s\"\n", filename);
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}

	if (dfs_findfree(dfs, (length + 256) / 256, &sector)) {
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}
	
	if (fread( dfs->raw_sect[sector], 1, length, fin) != length) {
		fprintf(stderr, "Error reading from \"%s\"\n", filename);
		ret = 2;
		goto out;
	}
	
	if (dfs_addcatent( dfs, bbcfilename, locked, load, exec, length, sector )) {
		fprintf(stderr, "Error updating catalogue : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}
	
	if (dfs_write( dfs, imagefilename ) ) {
		fprintf(stderr, "Could not write disc : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}
	
out:
	if (fin) fclose(fin);
	if (finf) fclose(finf);
	if (bbcfilename) free(bbcfilename);
	if (inffilename) free(inffilename);
	if (filename) free(filename);
	if (dfs) dfs_close(dfs);
	
	return ret;

}

int doread(const char *filename
,	const char *wildcard
,	const char *destdir
,	int inffile) {

	char disc_name[20];
	unsigned int sectors;
	unsigned int no_writes;
	unsigned int opt4;
	unsigned int cat_count;
	int i;
	char file_name[20];
	int locked;
	unsigned long int load, exec, len;
	unsigned int sector;

	char outfile[MAXFILENAME];
	
	FILE *fp;
	
	
	union dfs_disc *dfs = dfs_open(filename);
	if (!dfs) {
		fprintf(stderr, "Could not open disc %s\n", filename);
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		return 1;
	}

	dfs_get_disc_info(dfs, disc_name, &sectors, &no_writes, &opt4, &cat_count);

	for (i=0; i<(int)cat_count; i++) {
		dfs_get_file_info(
			dfs
		,	i
		,	file_name
		,	&locked
		,	&load
		,	&exec
		,	&len
		,	&sector
		);
	
			
		if (!matchwildcard(file_name, wildcard)) {
			
			if (inffile) {
				makefilename(outfile, destdir, file_name, "inf");
								
				fp = fopen(outfile, "w");
				if (!fp) {
					fprintf(stderr, "Cannot open \"%s\"\n", outfile);
					return 2;
				}
				
				fprintf(fp, "%-9.9s", file_name);
				fprintf(fp, " %6.6lX", load);
				fprintf(fp, " %6.6lX", exec);
				fprintf(fp, " %3.3lX", len);
				if (locked)
					fprintf(fp, " Locked");
				fprintf(fp, " CRC=%4.4X\n", crc(dfs->raw_sect[sector], len) );
								
				fclose(fp);
			}
			
			makefilename(outfile, destdir, file_name, NULL);
			
			fp = fopen(outfile, "wb");
			if (!fp) {
				fprintf(stderr, "Cannot open \"%s\"\n", outfile);
				dfs_close(dfs);
				return 2;
			}
			
			if (fwrite(dfs->raw_sect[sector], 1, len, fp) != len) {
				fprintf(stderr, "IO error on file \"%s\"\n", outfile);
				fclose(fp);
				dfs_close(dfs);
				return 3;
			}
			
			fclose(fp);
		}
	}
	
	dfs_close(dfs);
	
	return 0;	
}

int doinfo(const char *filename, const char *wildcard) {
	char disc_name[20];
	unsigned int sectors;
	unsigned int no_writes;
	unsigned int opt4;
	unsigned int cat_count;
	int i;
	char file_name[20];
	int locked;
	unsigned long int load, exec, len;
	unsigned int sector;

	union dfs_disc *dfs = dfs_open(filename);
	if (!dfs) {
		fprintf(stderr, "Could not open disc %s\n", filename);
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		exit(1);
	}
	
	
	
	dfs_get_disc_info(dfs, disc_name, &sectors, &no_writes, &opt4, &cat_count);
	
//	printf("%s (%u)\n", disc_name, no_writes);
//	printf("Drive %13s Option %u\n", "Elite.img", opt4);
//	printf("Sectors: %u\n", sectors);
	
	for (i=0; i<(int)cat_count; i++) {
		dfs_get_file_info(
			dfs
		,	i
		,	file_name
		,	&locked
		,	&load
		,	&exec
		,	&len
		,	&sector
		);
	
		if (!matchwildcard(file_name, wildcard)) {
			printf("%-9s  %c  %6.6lX %6.6lX %6.6lX %3.3X\n"
			, file_name, (locked)?'L':' ', load, exec, len, sector); 
		}
			
	}
	
	dfs_close(dfs);
	
	return 0;	
}

int doopt4(const char *filename, int opt4)
{
	int ret = 0;
	union dfs_disc *dfs = dfs_open(filename);
	if (!dfs) {
		fprintf(stderr, "Could not open disc %s\n", filename);
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		exit(1);
	}
	
	dfs_opt4(dfs, opt4);

	if (dfs_write( dfs, filename ) ) {
		fprintf(stderr, "Could not write disc : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}

out:
	dfs_close(dfs);
	return ret;
}

int dotitle(const char *filename, const char *title)
{
	int ret = 0;
	union dfs_disc *dfs = dfs_open(filename);
	if (!dfs) {
		fprintf(stderr, "Could not open disc %s\n", filename);
		fprintf(stderr, "%s\n", dfs_error_message(dfs_errno));
		exit(1);
	}
	
	dfs_title(dfs, title);
	
	if (dfs_write( dfs, filename ) ) {
		fprintf(stderr, "Could not write disc : %s\n", dfs_error_message(dfs_errno));
		ret = 1;
		goto out;
	}

out:
	dfs_close(dfs);
	return ret;
}


void doversion(FILE *f) {
	fprintf(f, "DFS version %s\n", PACKAGE_STRING);
}


void usage(FILE *f) {
	fprintf(f,
		"Usage: dfs <command> [<options>] <image> [<files to add>]\n"
		"\n"
		"Command is one of:\n"
		" help			print this message\n"
		" info			catalogue disc\n"
		" read			read file(s)\n"
		" add			add file(s)\n"
		" opt4			set boot action\n"
		" title			set title to first filename"
		" form			format i.e. create new image\n"
		" version		print version information and exit\n"
		"\n"
		"Info options:\n"
		" -w <wildcard>		see read\n"
		"\n"
		"Read options:\n"
		" -w <wildcard>		*info type wildcards (you'll need to put them\n"
		"			in single quotes to stop expansion) or filename\n"
		" -d <dir>		Dump to specified directory\n"
		" -i			Create " EXTSEP2 "inf files\n"
		"\n"
		"Form options:\n"
		" -40			Create a 40 track disc\n"
		" -80			Create an 80 track disc (default)\n"
		"\n"
		"Add options: 	For add operation <image> is followed by a list of files\n"
		"             	to add. The files can either be files or their associated " EXTSEP2 "inf\n"
		"		files. i.e. *" EXTSEP2 "inf. If an inf file is specified will, look for\n"
		"		a file without the " EXTSEP2 "inf extension and vice versa.\n"
		" -l			Load address - in hex i.e. 000E00 overrides inf file\n"
		" -e			Exec address - ditto -\n"
		" -f			BBC file name - overrides inf file\n"
		" -k			File is locked - overrides inf\n"
		" -u			File is unlocked - overrides inf\n"
		"		You must either supply a " EXTSEP2 "inf file with a filename in it or\n"
		"		pass the filename parameter\n"
		"		Be careful adding names, very little filename checking is done\n"
		"		and corrupt discs may be produced. This is so in the hope that\n"
		"		it might be of help in creating \"copy protected\" discs???\n"
		"		and its late at night and I just want it to work now...\n"
		"opt4 options:\n"
		" -0            ignore !BOOT\n"
		" -1            *LOAD !BOOT\n"
		" -2            *RUN !BOOT\n"
		" -3            *EXEC !BOOT\n"
	);
}

#define CHECK1MORE \
	if (i > argc - 1) { \
		fprintf(stderr, "Missing parameter\n"); \
		usage(stderr); \
		return 1; \
	} 
#define UKOPT \
		{fprintf(stderr, "Unknown option %s\n", argv[i]); \
		usage(stderr); \
		return 1;}

#define CHECKLAST \
	if (i != argc-1) { \
		fprintf(stderr, "Disk image name must be last parameter\n"); \
		usage(stderr); \
		return 1; \
	}

	
int argadd(int argc, char *argv[]) {
	
	unsigned long int load=0xF000000;
	unsigned long int exec =0xF000000;
	const char *bbcfile = NULL;
	const char *image = NULL;
	int ret;
	int locked = -1;
		
	int i=2;
	
	while (i<argc && argv[i][0]=='-') {
		if (strcmp(argv[i], "-l") == 0 ) {
			CHECK1MORE;
			load = strtol(argv[++i], NULL, 16);			
		} else if (strcmp(argv[i], "-e") == 0 ) {
			CHECK1MORE;
			exec = strtol(argv[++i], NULL, 16);			
		} else if (strcmp(argv[i], "-k") == 0 ) {
			locked = 1;
		} else if (strcmp(argv[i], "-u") == 0 ) {
			locked = 0;
		} else if (strcmp(argv[i], "-f") == 0 ) {
			CHECK1MORE;
			bbcfile = argv[++i];
		} else 
			UKOPT;
		i++;
	}

	CHECK1MORE;
	image = argv[i++];
	
	CHECK1MORE;
	while (i<argc) {
		ret = doadd(image, load, exec, locked, bbcfile, argv[i++]);
		if (ret) return ret;
	}		
	return 0;
		
}
		
int argread(int argc, char *argv[]) {
	
	const char *wildcard="*.*";
	const char *destdir=NULL;
	int inffiles=0;
	
	int i=2;
	
	while (i<argc && argv[i][0]=='-') {
		if (strcmp(argv[i], "-w") == 0 ) {
			CHECK1MORE;
			wildcard = argv[++i];
		} else if (strcmp(argv[i], "-d") == 0 ) {
			CHECK1MORE;
			destdir = argv[++i];
		} else if (strcmp(argv[i], "-i") == 0 ) {
			inffiles = 1;
		} else 
			UKOPT;
		i++;
	}

	CHECKLAST;
		
	return doread(argv[i], wildcard, destdir, inffiles);	
}
	
int arginfo(int argc, char *argv[]) {
	
	int i=2;
	const char *wildcard="*.*";
		
	while (i<argc && argv[i][0]=='-') {
		if (strcmp(argv[i], "-w") == 0 ) {
			CHECK1MORE;
			wildcard = argv[++i];
		} else 
			UKOPT;
		i++;
	}
	
	CHECKLAST;
	
	return doinfo(argv[i], wildcard);

}

int argform(int argc, char *argv[]) {
	
	int i = 2;
	int tracks = 80;
		
	while (i<argc && argv[i][0]=='-') {
		if (strcmp(argv[i], "-40") == 0 ) {
			tracks = 40;
		} else if (strcmp(argv[i], "-80") == 0 ) {
			tracks = 80;
		} else 
			UKOPT;
		i++;
	}
	
	CHECKLAST;
	
	return doform(argv[i], tracks);

}

int argopt4(int argc, char *argv[]) {
	
	int i = 2;
	int opt4 = 0;
		
	while (i<argc && argv[i][0]=='-') {
		if (strcmp(argv[i], "-0") == 0 ) {
			opt4 = 0;
		} else if (strcmp(argv[i], "-1") == 0 ) {
			opt4 = 1;
		} else if (strcmp(argv[i], "-2") == 0 ) {
			opt4 = 2;
		} else if (strcmp(argv[i], "-3") == 0 ) {
			opt4 = 3;
		} else 
			UKOPT;
		i++;
	}
	
	CHECKLAST;
	
	return doopt4(argv[i], opt4);
}

int argtitle(int argc, char *argv[]) {
	
	int i = 2;
	int opt4 = 0;
	char *image;
		
	while (i<argc && argv[i][0]=='-') {
		UKOPT;
		i++;
	}
	
	CHECK1MORE;
	image = argv[i++];

	CHECKLAST;
	
	return dotitle(image, argv[i]);
}


int main(int argc, char *argv[]) {
	
	if (argc<2) {
		fprintf(stderr, "Not enough parameters\n");
		usage(stderr);
		return 1;
	}
	
	if (strcmp(argv[1], "info")==0) {
		return arginfo(argc, argv);
	} else if (strcmp(argv[1], "read")==0) {
		return argread(argc, argv);
	} else if (strcmp(argv[1], "add")==0) {
		return argadd(argc, argv);
	} else if (strcmp(argv[1], "form")==0) {
		return argform(argc, argv);
	} else if (strcmp(argv[1], "opt4")==0) {
		return argopt4(argc, argv);	
	} else if (strcmp(argv[1], "title")==0) {
		return argtitle(argc, argv);	
	} else if (strcmp(argv[1], "help")==0) {
		usage(stderr);
		return 0;
	} else if (strcmp(argv[1], "version")==0) {
		doversion(stdout);
		return 0;
	} else {
		fprintf(stderr, "Unrecognised command\n");
		usage(stderr);
		return 1;
	}
}
