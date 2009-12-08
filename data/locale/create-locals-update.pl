#! /usr/bin/perl

# This proggie takes a (possibly outdated) locale file as its (only)
# argument, and creates a new version, with `-work` appended. The thus
# created tile contains all keys of the master file, and is intended
# to, after manual editing, replace the outdated locale file.

# Written by Barf on 2005-12-10.

$masterfilename = "deutsch.locale";

$#ARGV == 0 || die("Usage: create-locals-update.pl file.locale.");

$no_errors = 0;
$last_was_ok = 1;
$localefilename = @ARGV[0];
$outfilename = $localefilename . "-work";

open(masterfile, $masterfilename) || die("Could not open master file");
open(localefile, $localefilename) || die("Could not open locale file");
open(outfile, ">" . $outfilename) || die("Could not open output file");

while (<masterfile>) {
    $masterline = $_;
    ($masterkey) = /([^ ]+)/;
    ($junk, $mastertext) = /([^ ]+)[ ]+([^\n]+)/;
    if ($last_was_ok) {
	$localeline = <localefile>;
	chop $localeline;
	($localekey) = ($localeline =~ /([^ ]+)/,$localline);
	($junk, $localetext) = ($localeline =~ /([^ ]+)[ ]+([^\n]+)/);
    };
    if ($masterkey eq $localekey) {
	print outfile $localeline, "\n";
	$last_was_ok = 1;
    } else {
	$no_errors++;
	#print "|", $masterkey, "|", $mastertext, "|", $localekey, "|", $localetext, "|\n";
	print outfile $masterkey, " TRANSLATE ", $mastertext, "\n";
	$last_was_ok = 0;
    }
}

close(outfile);

print "There were ", $no_errors, " error(s).\n";

if ($no_errors == 0) {
    unlink($outfilename);
}
