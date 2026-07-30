#!/usr/bin/perl

$first_line=`git log -1 --pretty=%B | head -1`;

if ($first_line =~ /\[Bug Fix\]/) {
} elsif ($first_line =~ /\[Build Fix\]/) {
} elsif ($first_line =~ /\[API Change\]/) {
} elsif ($first_line =~ /\[New Architecture Support\]/) {
} elsif ($first_line =~ /\[Optimization\]/) {
} elsif ($first_line =~ /\[Cosmetic Change\]/) {
} elsif ($first_line =~ /\[New Feature\]/) {
} elsif ($first_line =~ /\[Build System Change\]/) {
} elsif ($first_line =~ /\[Jenkins Change\]/) {
} else {
	printf("*************************************************************\n");
	printf("*************************************************************\n");
	printf("Commit message check error!\n");
	printf("Commit message must include one of the following:\n");
	printf("\t[Bug Fix]\n");
	printf("\t[Build Fix]\n");
	printf("\t[API Change]\n");
	printf("\t[New Architecture Support]\n");
	printf("\t[Optimization]\n");
	printf("\t[Cosmetic Change]\n");
	printf("\t[New Feature]\n");
	printf("\t[Build System Change]\n");
	printf("\t[Jenkins Change]\n");
	printf("*************************************************************\n");
	printf("*************************************************************\n");
	exit(-1);
}

