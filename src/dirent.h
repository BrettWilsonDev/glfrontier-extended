/*
 * static char rcsid[] = "$Id: dirent.h,v 1.3 2008/04/06 18:43:58 jullien Exp $";
 */

/*
 * This  program  is  free  software;  you can redistribute it and/or
 * modify  it  under  the  terms of the GNU General Public License as
 * published  by  the  Free  Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This  program  is  distributed in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY;  without  even the implied  warranty of
 * MERCHANTABILITY  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You  should have received a copy of the GNU General Public License
 * along  with  this  program;  if  not,  write  to the Free Software
 * Foundation,  Inc.,  59  Temple  Place  -  Suite  330,  Boston,  MA
 * 02111-1307, USA.
 */

#ifndef __DIRENT_H
#define __DIRENT_H

#if defined(_WIDECHARS)
#if !defined(_UNICODE)
#define _UNICODE
#endif
#if !defined(UNICODE)
#define UNICODE
#endif
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

	/*
	 * Definitions for library routines operating on directories.
	 */

#if defined(_WIN32) || defined(_WIN64)
#include <limits.h>
#include <windows.h>
#if !defined(_WINDOWS_SOURCE)
#define _WINDOWS_SOURCE
#endif
#endif

#if defined(_OS2)
#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#include <os2.h>
#if !defined(INCL_16)
	typedef ULONG UCOUNT;
#else
	typedef USHORT UCOUNT;
#endif
	static UCOUNT count;
#endif

#if !defined(_WINDOWS_SOURCE) && !defined(_OS2)
#include <dos.h>
	typedef struct _find_t DOSFINDBUF;
#endif

#if defined(_UNICODE)
	typedef wchar_t _char_t;
#define _pstrcpy wcscpy
#define _pstrcat wcscat
#define _pstrlen wcslen
#define _STR(s) (L##s)
#else
typedef char _char_t;
#define _pstrcpy strcpy
#define _pstrcat strcat
#define _pstrlen strlen
#define _STR(s) (s)
#endif

#if !defined(NAME_MAX)
#if defined(FILENAME_MAX)
#define NAME_MAX FILENAME_MAX
#else
#if defined(_DOS)
#define NAME_MAX 13
#else
#define NAME_MAX 255
#endif /* _DOS		*/
#endif /* FILENAME_MAX */
#endif /* NAME_MAX	*/

#define I_STD 0x0000 /* Normal file - No restrictions */
#define I_RDO 0x0001 /* Read only file		 */
#define I_HID 0x0002 /* Hidden file 			 */
#define I_SYS 0x0004 /* System file			 */
#define I_LAB 0x0008 /* Volume ID file		 */
#define I_DIR 0x0010 /* Subdirectory			 */
#define I_ARC 0x0020 /* Archive file			 */

#define FINDATTRIB I_STD | I_RDO | I_HID | I_DIR

	struct dirent
	{
		unsigned long d_ino;	 /* inode number of entry */
		unsigned short d_namlen; /* length of d_name	 */
		/*
		 *	POSIX defined field
		 */
		_char_t d_name[NAME_MAX]; /* filename              */
	};

#define d_reclen d_namlen

#if defined(_WINDOWS_SOURCE)
#define FINDHANDLE HANDLE
#define FINDDATA WIN32_FIND_DATA
#define filename cFileName
#define SysCloseDir(hdir) FindClose(hdir);
#endif

#if defined(_OS2)
#define FINDHANDLE unsigned int
#if defined(_OS2V1)
#define FINDDATA FILEFINDBUF
#else
#define FINDDATA FILEFINDBUF3
#endif
#define filename achName
#define SysCloseDir(hdir) DosFindClose((HDIR)hdir);
#endif

#if (defined(_DOS) || defined(MSDOS)) && !defined(_WINDOWS_SOURCE) && !defined(_OS2)
#define FINDHANDLE unsigned int
#define FINDDATA DOSFINDBUF
#define filename name
#define SysCloseDir(hdir)
#endif

	typedef struct _dirdesc
	{
		/*
		 *	DIR public (portable) interface
		 */
		unsigned int status;	  /* Status flag		  */
		long dd_loc;			  /* Current location	  */
		_char_t *dd_path;		  /* Path name		  */
		struct dirent *dd_direct; /* Pointer to direct	  */
		/*
		 *	DIR private system interface
		 */
		FINDHANDLE dd_hfind; /* Handle to find	  */
		FINDDATA dd_resbuf;	 /* Find data		  */
	} DIR;

	extern DIR *opendir(const _char_t *file);
	extern struct dirent *readdir(DIR *dirp);
	extern void rewinddir(DIR *dirp);
	extern int closedir(DIR *dirp);
#if !defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)
	extern long telldir(DIR *dirp);
	extern void seekdir(DIR *dirp, long loc);
#endif

#if defined(__cplusplus)
}
#endif

#endif
