/*
 * exempi - exempi.cpp
 *
 * Copyright (C) 2011 Hubert Figuiere
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1 Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2 Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * 3 Neither the name of the Authors, nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software wit hout specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <exempi/xmp.h>


enum {
	ACTION_NONE = 0,
	ACTION_SET,
	ACTION_GET
};

void process_file(const char * filename, bool no_reconcile, bool dump_xml, bool write_in_place,
	int action, const std::string & value_name, const std::string & prop_value, const std::string & output);


void fatal_error(const char * error)
{
	fprintf(stderr, "ERROR: %s\n", error);
	exit(128);
}


/** Print the usage, and quit. Exit code is 255.
 */
void usage()
{
	fprintf(stderr, "Exempi version %s\n", VERSION);
	fprintf(stderr, "exempi { -h | [ -R ] [ -x ] [ { -w | -o <file> } ] [ { -g <prop_name> | -s <prop_name> -v <value> }  ] } <files>\n");
	fprintf(stderr, "\t-h: show this help\n");
	fprintf(stderr, "\t-R: don't reconcile\n");
	fprintf(stderr, "\t-x: dump XML\n");
	fprintf(stderr, "\t-w: write in place. Only for -s. Not compatible with -o.\n");
	fprintf(stderr, "\t-o <file>: file to write the output to.\n");
	fprintf(stderr, "\t-g <prop_name>: retrieve the value with prop_name.\n");
	fprintf(stderr, "\t-s <prop_name> -v <value>: retrieve or get the value.\n");
	fprintf(stderr, "\t<files> the files to read from.\n");
	
	exit(255);
}

int main(int argc, char **argv)
{
	int ch;
	
	int action = ACTION_NONE;
	bool dont_reconcile = false;
	bool write_in_place = false;
	bool dump_xml = false;
	std::string output_file;
	std::string value_name;
	std::string prop_value;
	
	while((ch = getopt(argc, argv, "hRo:wxg:s:v:")) != -1) {
		switch(ch) {
		
		case 'R':
			dont_reconcile = true;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'w':
			write_in_place = true;
			break;
		case 'x':
			dump_xml = true;
			break;
		case 'g':
			if(!value_name.empty()) {
				usage();
			}
			action = ACTION_GET;
			value_name = optarg;
			break;
		case 's':
			if(!value_name.empty()) {
				usage();
			}
			action = ACTION_SET;
			value_name = optarg;
			break;
		case 'v':
			if(action != ACTION_SET || value_name.empty()) {
				fatal_error("-v needs to come after -s");
			}
			prop_value = optarg;
			break;
		case 'h':
		case '?':
			usage();
			break;
		}
	}
	
	argc -= optind;
    argv += optind;
    
    if(optind == 1 || argc == 0) {
		fatal_error("Missing input.");
    	return 1;
    }
    
    if(write_in_place && (!output_file.empty() || action != ACTION_SET)) {
    	fatal_error("Need to write in place and output file are mutually exclusive.");
    	return 1;
    }
    
    if(action == ACTION_SET && (!write_in_place && argc > 1)) {
    	fatal_error("Need to write in place for more than one file.");
    	return 1;
    }

	xmp_init();
	while(argc) {
	
		process_file(*argv, dont_reconcile, write_in_place, dump_xml, action, value_name, prop_value, output_file);
	
		argc--;
		argv++;
	}
    
    xmp_terminate();
    return 0;
}


/** dump the XMP xml to the output IO */
void dump_xmp(const char *filename, bool no_reconcile, FILE *outio)
{
	printf("dump_xmp for file %s\n", filename);
	XmpFilePtr f = xmp_files_open_new(filename, 
					(XmpOpenFileOptions)(XMP_OPEN_READ | (no_reconcile ? XMP_OPEN_ONLYXMP : 0)));
	if(f) {
		XmpPtr xmp = xmp_new_empty();
		xmp_files_get_xmp(f, xmp);
	
		XmpStringPtr output = xmp_string_new();
	
		xmp_serialize_and_format(xmp, output, 
								 XMP_SERIAL_OMITPACKETWRAPPER, 
								 0, "\n", " ", 0);
								 
		fprintf(outio, "%s", xmp_string_cstr(output));
			
		xmp_string_free(output);
		xmp_free(xmp);
		xmp_files_close(f, XMP_CLOSE_NOOPTION);
		xmp_files_free(f);
	}
}

/** get an xmp prop and dump it to the output IO */
void get_xmp_prop(const char * filename, const std::string & value_name, bool no_reconcile, FILE *outio)
{
	XmpFilePtr f = xmp_files_open_new(filename, 
					(XmpOpenFileOptions)(XMP_OPEN_READ | (no_reconcile ? XMP_OPEN_ONLYXMP : 0)));
	if(f) {
		XmpPtr xmp = xmp_new_empty();
		xmp_files_get_xmp(f, xmp);

		uint32_t prop_bits;
		
		std::string prefix;
		size_t idx = value_name.find(':');
		if(idx != std::string::npos) {
			prefix = std::string(value_name, 0, idx);
		}
				
		XmpStringPtr property = xmp_string_new();
		XmpStringPtr ns = xmp_string_new();
		xmp_prefix_namespace_uri(prefix.c_str(), ns);
		xmp_get_property(xmp, xmp_string_cstr(ns), value_name.c_str(), property, &prop_bits);
		fprintf(outio, "%s\n", xmp_string_cstr(property));
		xmp_string_free(ns);
		xmp_string_free(property);

		xmp_free(xmp);
		xmp_files_close(f, XMP_CLOSE_NOOPTION);
		xmp_files_free(f);
	}
}


void set_xmp_prop(const char * filename, const std::string & value_name, const std::string & prop_value,
	bool no_reconcile, bool write_in_place, FILE *outio)
{
	XmpFilePtr f = xmp_files_open_new(filename, 
					(XmpOpenFileOptions)((write_in_place ? XMP_OPEN_FORUPDATE : XMP_OPEN_READ) 
						| (no_reconcile ? XMP_OPEN_ONLYXMP : 0)));
	if(f) {
		XmpPtr xmp = xmp_files_get_new_xmp(f);

		std::string prefix;
		size_t idx = value_name.find(':');
		if(idx != std::string::npos) {
			prefix = std::string(value_name, 0, idx);
		}

		XmpStringPtr ns = xmp_string_new();
		xmp_prefix_namespace_uri(prefix.c_str(), ns);
		if(!xmp_set_property(xmp, xmp_string_cstr(ns), value_name.c_str() + idx + 1, prop_value.c_str(), 0)) {
			fprintf(stderr, "set error = %d\n", xmp_get_error());
		}

		xmp_string_free(ns);

		if(write_in_place) {
			if(!xmp_files_can_put_xmp(f, xmp)) {
				fprintf(stderr, "can put xmp error = %d\n", xmp_get_error());		 				
			}
		 	if(!xmp_files_put_xmp(f, xmp)) {
				fprintf(stderr, "put xmp error = %d\n", xmp_get_error());		 	
		 	}
			if(!xmp_files_close(f, XMP_CLOSE_SAFEUPDATE)) {
				fprintf(stderr, "close error = %d\n", xmp_get_error());
			}
		}		
		xmp_free(xmp);
		xmp_files_free(f);
	}
}


void process_file(const char * filename, bool no_reconcile, bool write_in_place, bool dump_xml,
	int action, const std::string & value_name, const std::string & prop_value,
	const std::string & output)
{
	printf("processing file %s\n", filename);

	FILE *outio = stdout;
	
	bool needclose = false;
	if(!output.empty()) {
		outio = fopen(output.c_str(), "a+");
		needclose = true;
	}
	
	switch (action) {
	case ACTION_NONE:
		if(dump_xml) {
			dump_xmp(filename, no_reconcile, outio);
		}
		break;	
	case ACTION_SET:
		set_xmp_prop(filename, value_name, prop_value, no_reconcile, write_in_place, outio);
		break;
	case ACTION_GET:
		get_xmp_prop(filename, value_name, no_reconcile, outio);
		break;
	default:
		break;
	}

	if(needclose) {
		fclose(outio);
	}
}