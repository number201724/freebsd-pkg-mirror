#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <jansson.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <sys/queue.h>
#include <iconv.h>

typedef struct package_s
{
	LIST_ENTRY( package_s ) pos;
	bool fillable;
	char *name;
	char *origin;
	char *version;
	char *abi;
	char *arch;
	char *sum;
	char *path;
	char *repopath;
} package_t;

sqlite3 *dbi;
LIST_HEAD( package_listhead, package_s ) g_packages = LIST_HEAD_INITIALIZER( g_packages );
LIST_HEAD( jobs_isthead, package_s ) g_jobs = LIST_HEAD_INITIALIZER( g_jobs );
LIST_HEAD( clean_isthead, package_s ) g_clean = LIST_HEAD_INITIALIZER( g_clean );


const char *g_baseurl;
const char *g_abi;
const char *g_channel;
const char *g_directory;


void pkg_free( package_t *pkg )
{
	if( pkg->name )
	{
		free( pkg->name );
		pkg->name = NULL;
	}

	if( pkg->origin )
	{
		free( pkg->origin );
		pkg->origin = NULL;
	}

	if( pkg->version )
	{
		free( pkg->version );
		pkg->version = NULL;
	}

	if( pkg->abi )
	{
		free( pkg->abi );
		pkg->abi = NULL;
	}

	if( pkg->arch )
	{
		free( pkg->arch );
		pkg->arch = NULL;
	}
	if( pkg->sum )
	{
		free( pkg->sum );
		pkg->sum = NULL;
	}
	if( pkg->path )
	{
		free( pkg->path );
		pkg->path = NULL;
	}
	if( pkg->repopath )
	{
		free( pkg->repopath );
		pkg->repopath = NULL;
	}

	free( pkg );
}

package_t *pkg_new( const char *name, const char *origin, const char *version, const char *abi,
					const char *arch, const char *sum, const char *path, const char *repopath )
{
	package_t *pkg;

	pkg = (package_t *)malloc( sizeof( package_t ) );

	pkg->name = strdup( name );
	pkg->origin = strdup( origin );
	pkg->version = strdup( version );
	pkg->abi = strdup( abi );
	pkg->arch = strdup( arch );
	pkg->sum = strdup( sum );
	pkg->path = strdup( path );
	pkg->repopath = strdup( repopath );

	return pkg;
}


int _pkg_query( void *a_param, int argc, char **argv, char **column )
{
	package_t *package;
	package = (package_t *)a_param;

	package->fillable = true;

	for( int i = 0; i < argc; i++ )
	{
		if( strcmp( column[i], "name" ) == 0 )
		{
			package->name = strdup( argv[i] );
		}
		if( strcmp( column[i], "origin" ) == 0 )
		{
			package->origin = strdup( argv[i] );
		}
		if( strcmp( column[i], "version" ) == 0 )
		{
			package->version = strdup( argv[i] );
		}
		if( strcmp( column[i], "abi" ) == 0 )
		{
			package->abi = strdup( argv[i] );
		}
		if( strcmp( column[i], "arch" ) == 0 )
		{
			package->arch = strdup( argv[i] );
		}
		if( strcmp( column[i], "sum" ) == 0 )
		{
			package->sum = strdup( argv[i] );
		}
		if( strcmp( column[i], "path" ) == 0 )
		{
			package->path = strdup( argv[i] );
		}
		if( strcmp( column[i], "repopath" ) == 0 )
		{
			package->repopath = strdup( argv[i] );
		}
	}

	return 0;
}

package_t *pkg_query( const char *name )
{
	char sql[1024];
	char *error;
	int code;
	package_t pkg;
	package_t *newpkg;

	memset( &pkg, 0, sizeof( pkg ) );
	sprintf( sql, "SELECT * FROM package WHERE name = \"%s\"", name );

	code = sqlite3_exec( dbi, sql, _pkg_query, &pkg, &error );

	if( code == 0 && pkg.fillable )
	{
		newpkg = (package_t *)malloc( sizeof( package_t ) );

		memcpy( newpkg, &pkg, sizeof( package_t ) );

		return newpkg;
	}


	return NULL;
}

void pkg_update( const char *name, const char *origin, const char *version, const char *abi,
				 const char *arch, const char *sum, const char *path, const char *repopath )
{
	char sql[1024];
	char *error;
	int code;
	sprintf( sql, "DELETE FROM package WHERE name = \"%s\"", name );
	sqlite3_exec( dbi, sql, NULL, NULL, &error );


	sprintf( sql, "INSERT INTO package VALUES(\"%s\",\"%s\",\"%s\",\"%s\","
			 "\"%s\",\"%s\",\"%s\",\"%s\");",
			 name, origin, version, abi,
			 arch, sum, path, repopath
	);

	sqlite3_exec( dbi, sql, NULL, NULL, &error );
}


void addjob( const char *name, const char *origin, const char *version, const char *abi,
			 const char *arch, const char *sum, const char *path, const char *repopath )
{
	package_t *pkg = pkg_new( name, origin, version, abi, arch, sum, path, repopath );

	LIST_INSERT_HEAD( &g_jobs, pkg, pos );
}

void addcleanjob( const char *name, const char *origin, const char *version, const char *abi,
				  const char *arch, const char *sum, const char *path, const char *repopath )
{
	package_t *pkg = pkg_new( name, origin, version, abi, arch, sum, path, repopath );

	LIST_INSERT_HEAD( &g_clean, pkg, pos );
}

void load_packagesite( void )
{
	static char line[65535];
	static char line2[65535];
	json_error_t error;
	json_t *json;
	int code;
	FILE *hfile;
	const char *name;
	const char *origin;
	const char *version;
	const char *abi;
	const char *arch;
	const char *sum;
	const char *path;
	const char *repopath;
	package_t *pkg, *cachepkg;
	size_t outbytes, inbytes;
	char *pline2, *pline;

	hfile = fopen( "packagesite.yaml", "r" );

	if( hfile == NULL )
	{
		printf( "open packagesite.yaml failed. err:%d\n", errno );
		exit( -1 );
	}

	while( fgets( line, sizeof( line ) - 1, hfile ) )
	{
		line[sizeof( line ) - 1] = '\0';

		json = json_loads( line, 0, &error );

		if( json == NULL )
		{
			memset(&line2, 0, sizeof(line2));
			iconv_t conv = iconv_open("UTF-8", "ISO-8859-1");
			if(conv == (iconv_t)-1)
			{
				printf( "iconv_open failed. %s\n", line );
				exit(-1);
			}

			pline = line;
			pline2 = line2;
			inbytes = strlen(line);
			outbytes = sizeof(line2)-1;

			if(iconv(conv, &pline, &inbytes, &pline2, &outbytes)==-1)
			{
				printf("iconv error\n");
				exit(-1);
			}
			
			iconv_close(conv);

			json = json_loads( line2, 0, &error );
			
			if(json == NULL)
			{
				printf( "json_loads failed. %s\n", line );
				exit( -1 );
			}

		}

		code = json_unpack( json,
							"{"
							"s:s"
							"s:s"
							"s:s"
							"s:s"
							"s:s"
							"s:s"
							"s:s"
							"s:s"
							"}",
							"name", &name,
							"origin", &origin,
							"version", &version,
							"abi", &abi,
							"arch", &arch,
							"sum", &sum,
							"path", &path,
							"repopath", &repopath
		);

		if( code != 0 )
		{
			printf( "json_unpack failed\n" );
			exit( -1 );
		}

		pkg = pkg_new( name, origin, version, abi, arch, sum, path, repopath );
		LIST_INSERT_HEAD( &g_packages, pkg, pos );

		json_decref( json );
	}

	fclose( hfile );
}


void load_update_packages( void )
{
	package_t *pkg, *pkg2;

	LIST_FOREACH( pkg, &g_packages, pos )
	{
		pkg2 = pkg_query( pkg->name );
		if( pkg2 == NULL )
		{
			addjob( pkg->name, pkg->origin, pkg->version, pkg->abi,
					pkg->arch, pkg->sum, pkg->path, pkg->repopath
			);
		}
		else
		{
			if( strcmp( pkg->sum, pkg2->sum ) != 0 || strcmp( pkg->version, pkg2->version ) != 0 )
			{
				addjob( pkg->name, pkg->origin, pkg->version, pkg->abi,
						pkg->arch, pkg->sum, pkg->path, pkg->repopath
				);

				printf( "[UPDATE][%s][%s][%s]\n", pkg->name, pkg->version, pkg->path );
			}

			pkg_free( pkg2 );
		}
	}
}

int _load_clean_packages( void *a_param, int argc, char **argv, char **column )
{
	package_t *pkg1, *pkg2;
	bool exists;

	pkg1 = (package_t *)malloc( sizeof( package_t ) );

	for( int i = 0; i < argc; i++ )
	{
		if( strcmp( column[i], "name" ) == 0 )
		{
			pkg1->name = strdup( argv[i] );
		}
		if( strcmp( column[i], "origin" ) == 0 )
		{
			pkg1->origin = strdup( argv[i] );
		}
		if( strcmp( column[i], "version" ) == 0 )
		{
			pkg1->version = strdup( argv[i] );
		}
		if( strcmp( column[i], "abi" ) == 0 )
		{
			pkg1->abi = strdup( argv[i] );
		}
		if( strcmp( column[i], "arch" ) == 0 )
		{
			pkg1->arch = strdup( argv[i] );
		}
		if( strcmp( column[i], "sum" ) == 0 )
		{
			pkg1->sum = strdup( argv[i] );
		}
		if( strcmp( column[i], "path" ) == 0 )
		{
			pkg1->path = strdup( argv[i] );
		}
		if( strcmp( column[i], "repopath" ) == 0 )
		{
			pkg1->repopath = strdup( argv[i] );
		}
	}

	LIST_FOREACH( pkg2, &g_packages, pos )
	{
		if( strcmp( pkg1->name, pkg2->name ) != 0 )
			continue;
		exists = true;
		break;
	}

	if( !exists )
	{
		addcleanjob( pkg1->name, pkg1->origin, pkg1->version, pkg1->abi,
					 pkg1->arch, pkg1->sum, pkg1->path, pkg1->repopath );

		printf( "[REMOVE][%s]\n", pkg1->name );
	}

	pkg_free( pkg1 );

	return 0;
}


void load_clean_packages( void )
{
	char sql[1024];
	char *error;
	int code;
	sprintf( sql, "SELECT * FROM package" );
	code = sqlite3_exec( dbi, sql, _load_clean_packages, NULL, &error );

	if( code != 0 )
	{
		printf( "prepare_remove_jobs failed\n" );
		exit( -1 );
	}
}



void process_clean_packages( void )
{
	package_t *pkg;
	char path[1024];

	LIST_FOREACH( pkg, &g_clean, pos )
	{
		sprintf( path, "%s/%s", g_directory, pkg->path );
		remove( path );
	}
}

bool dlfile( char *url, char *path )
{
	pid_t pid;
	int code;
	int status;

	char *argv[] = { "curl", "--create-dirs", "-o", path, url, NULL };
	char *env[] = { NULL };

	code = posix_spawnp( &pid, "curl", NULL, NULL, argv, env );

	if( code == 0 )
	{
		waitpid( pid, &status, 0 );

		if( status == 0 )
			return true;

	}

	return false;
}

void byte2hex( unsigned char c, char *o )
{
	char numbers[] = "0123456789ABCDEF";

	o[1] = numbers[(c & 0xF)];
	o[0] = numbers[((c & 0xF0) >> 4)];
}

bool validate_file_digest( const char *path, const char *digest )
{
	SHA256_CTX ctx;
	FILE *fhandle;
	long size;
	void *pdata;
	unsigned char md[SHA256_DIGEST_LENGTH];
	char file_digest[SHA256_DIGEST_LENGTH * 2 + 1];

	fhandle = fopen( path, "rb" );
	if( fhandle == NULL )
		return false;

	fseek( fhandle, 0, SEEK_END );
	size = ftell( fhandle );
	fseek( fhandle, 0, SEEK_SET );

	pdata = malloc( size );
	if( pdata == NULL )
	{
		fclose( fhandle );
		return false;
	}

	if( size != fread( pdata, 1, size, fhandle ) )
	{
		fclose( fhandle );
		return false;
	}

	fclose( fhandle );

	SHA256_Init( &ctx );
	SHA256_Update( &ctx, pdata, size );
	SHA256_Final( md, &ctx );

	free( pdata );

	for( int i = 0; i < SHA256_DIGEST_LENGTH; i++ )
	{
		byte2hex( md[i], &file_digest[i * 2] );
	}

	file_digest[SHA256_DIGEST_LENGTH*2] = '\0';

	printf( "%s => %s\n", file_digest , digest );

	return strcasecmp( file_digest, digest ) == 0;
}

void writelog( const char *format, ... )
{
	va_list ap;
	FILE *logfile;

	va_start( ap, format );

	logfile = fopen( "failed.log", "a+" );
	if( logfile != NULL )
	{
		vfprintf( logfile, format, ap );
		fclose( logfile );
	}
}

void process_update_packages( void )
{
	package_t *pkg;
	char path[1024];
	char url[1024];

	// http://pkg0.kwc.freebsd.org/FreeBSD:12:amd64/quarterly/

	LIST_FOREACH( pkg, &g_jobs, pos )
	{
		sprintf( path, "%s/%s", g_directory, pkg->path );
		sprintf( url, "%s/%s/%s/%s", g_baseurl, g_abi, g_channel, pkg->path );

		printf( "[download][%s][%s]\n", path, url );

		remove( path );

		if( dlfile( url, path ) )
		{
			if( validate_file_digest( path, pkg->sum ) )
			{
				pkg_update( pkg->name, pkg->origin, pkg->version, pkg->abi,
							pkg->arch, pkg->sum, pkg->path, pkg->repopath
				);
				printf( "[OK][%s][%s]\n", path, url );
			}
			else
			{
				writelog( "[FAIL HASH][%s][%s]\n", path, url );
				printf( "[FAIL HASH][%s][%s]\n", path, url );
			}
		}
		// remove( path );
	}
}

void process_job( void )
{
	process_clean_packages( );
	process_update_packages( );
}


void init_schema( void )
{
	const char *sql;
	char *error;

	sql = "CREATE TABLE \"package\" ("
		"\"name\" text NOT NULL,"
		"\"origin\" TEXT,"
		"\"version\" TEXT,"
		"\"abi\" TEXT,"
		"\"arch\" TEXT,"
		"\"sum\" TEXT,"
		"\"path\" TEXT,"
		"\"repopath\" TEXT"
		");";

	sqlite3_exec( dbi, sql, NULL, NULL, &error );

	sql = "CREATE INDEX \"name\""
		"ON \"package\" ("
		"\"name\""
		");";

	sqlite3_exec( dbi, sql, NULL, NULL, &error );
}


bool extract_txz( char *path )
{
	pid_t pid;
	int code;
	int status;

	char *argv[] = { "tar", "Jxf", path, NULL };
	char *env[] = { NULL };

	code = posix_spawnp( &pid, "tar", NULL, NULL, argv, env );

	if( code == 0 )
	{
		waitpid( pid, &status, 0 );

		if( status == 0 )
			return true;

	}

	return false;
}

void download_packagesite(void)
{
	char url[1024];
	char path[1024];

	sprintf(url, "%s/%s/%s/packagesite.txz", g_baseurl, g_abi, g_channel);
	sprintf(path, "%s/packagesite.txz", g_directory);

	if(!dlfile(url, path))
	{
		printf("download packagesite.txz failed\n");
		exit(-1);
	}

	extract_txz( path );

	sprintf(url, "%s/%s/%s/meta.conf", g_baseurl, g_abi, g_channel);
	sprintf(path, "%s/meta.conf", g_directory);
	if(!dlfile(url, path))
	{
		printf("download meta.conf failed\n");
		exit(-1);
	}

	sprintf(url, "%s/%s/%s/meta.txz", g_baseurl, g_abi, g_channel);
	sprintf(path, "%s/meta.txz", g_directory);
	if(!dlfile(url, path))
	{
		printf("download meta.txz failed\n");
		exit(-1);
	}

}

int main( int argc, char *argv[] )
{
	int code;

	if(argc < 5)
	{
		printf("pkg-mirror <baseurl> <abi> <channel> <output>\n");
		printf("example: pkg-mirror \"http://pkg0.kwc.freebsd.org\" \"FreeBSD:12:amd64\" \"quarterly\" \"/data/pkg\"\n");
		return 0;
	}

	
	g_baseurl = argv[1];
	g_abi = argv[2];
	g_channel = argv[3];
	g_directory = argv[4];

	if(strstr(g_baseurl, "http") == NULL)
	{
		printf("invalid baseurl\n");
		return -1;
	}
	if(strstr(g_abi, "FreeBSD:") == NULL)
	{
		printf("invalid abi\n");
		return -1;
	}

	if(strcmp(g_channel, "quarterly")!= 0 && strcmp(g_channel, "latest") != 0)
	{
		printf("invalid channel only allow:latest,quarterly\n");
		return -1;
	}

	//g_baseurl = "http://pkg0.kwc.freebsd.org";
	//g_abi = "FreeBSD:12:amd64";
	//g_channel = "quarterly";
	//g_directory = "/data/pkg2";
	code = sqlite3_open( "pkgmirror.db", &dbi );

	if( code != 0 )
	{
		printf( "sqlite_open failed err:%d\n", code );
		return -1;
	}


	init_schema( );

	download_packagesite();

	load_packagesite( );
	load_update_packages( );
	load_clean_packages( );

	process_job( );

	sqlite3_close( dbi );




	return 0;
}
