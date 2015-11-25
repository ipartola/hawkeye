
#include <stdio.h>

const char *program_name   = "hawkeye";
const char *version_string = "0.3";
const char *author_name    = "Igor Partola";
const char *email_address  = "igor@igorpartola.com";
const char *years          = "2013-2015";
const char *copyright      = "Copyright (C) 2013-2015 Igor Partola\n\
This program comes with ABSOLUTELY NO WARRANTY;\n\
This is free software, and you are welcome to redistribute it\n\
under terms of the GNU General Public License version 3 or later.";

void print_version() {
	printf("%s - %s\n\n%s\n", program_name, version_string, copyright);
}

