#!/usr/bin/perl

## The g++ -MM options has a bad feature that it does not produce correct path
## for target .o files, i.e. all files are assumed to reside in the current directory
## Therefore, we must correct this if we are to use automatically generated
## dependency files.

open (SRC_LIST, "find . -name \'*.cc\'|") || die "unable to open find pipe";



while (<SRC_LIST>) {
    ## the 'find' command returns strings of form './[a-zA-Z0-9]+'
    ## hence get rid of first two characters
    s/^..(.+)/\1/g;
    s/cc$/o/g;
    if (m|[/]|) {
	chop; 
	push @distant_sources, $_; 
    }
}

close SRC_LIST;

#foreach (@distant_sources) {
#    print ;#
#}

while($line = <STDIN>) {
    if ($line =~ m/^([A-Za-z_0-9.]+)/) {
	#print "$line";

	$filename = $1;

	##print "filename = $filename\n";

	@tmp = grep (/$filename/, @distant_sources);

	#foreach (@tmp) {
	#    print "tmp = $_\n";
	#}

	if ($#tmp == -1) { # empty array 

	}
	else  {
	    if ($#tmp == 0) {
		$line =~ s/$filename/$tmp[0]/;
	    } else {
		die "tmp has size $#tmp"; 
	    }
	}
    }

    print $line;
}

