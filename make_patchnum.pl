#!/usr/bin/perl
# These two should go upon release to make the script Perl 5.005 compatible
use strict;
use warnings;

=head1 NAME

make_patchnum.pl - make patchnum

=head1 SYNOPSIS

  miniperl make_patchnum.pl

  perl make_patchnum.pl

This program creates the files holding the information
about locally applied patches to the source code. The created
files are C<.patchnum>, C<unpushed.h> and C<lib/Config_git.pl>.

C<.patchnum> contains ???

C<lib/Config_git.pl> contains the configuration of git for
this branch.

C<unpushed.h> contains the local changes that haven't been
synchronized with the remote repository as configured with
C<< git configure branch.<current branch>.remote >>

=cut

BEGIN {
    my $root=".";
    while (!-e "$root/perl.c" and length($root)<100) {
        if ($root eq '.') {
	        $root="..";
    	} else {
	        $root.="/..";
	    }
    }
    die "Can't find toplevel" if !-e "$root/perl.c";
    sub path_to { "$root/$_[0]" } # use $_[0] if this'd be placed in toplevel.
}

sub read_file {
    my $file = path_to(@_);
    return "" unless -e $file;
    open my $fh, '<', $file
	or die "Failed to open for read '$file':$!";
    return do { local $/; <$fh> };
}

sub write_file {
    my ($file, $content) = @_;
    $file= path_to($file);
    open my $fh, '>', $file
	or die "Failed to open for write '$file':$!";
    print $fh $content;
    close $fh;
}

sub backtick {
    my $command = shift;
    my $result = `$command`;
    chomp $result;
    return $result;
}

my $existing_patchnum = read_file('.patchnum');
my $existing_config   = read_file('lib/Config_git.pl');
my $existing_unpushed = read_file('unpushed.h');

my $unpushed_commits = '/*no-op*/';
my ($read, $branch, $snapshot_created, $commit_id, $describe);
my ($changed, $extra_info, $commit_title, $new_patchnum);
if (my $patch_file= read_file('.patch')) {
    ($branch, $snapshot_created, $commit_id, $describe) = split /\s+/, $patchfile;
    $extra_info = "git_snapshot_date='$snapshot_created'";
    $commit_title = "Snapshot of:";
}
elsif (-d path_to('.git')) {
    # git branch | awk 'BEGIN{ORS=""} /\*/ { print $2 }'
    $branch = join "", map { (split /\s/, $_)[1] }
              grep {/\*/} split /\n/, backtick('git branch');
    my $remote;
    if (length $branch) {
        $remote = backtick("git config branch.$branch.remote");
    }
    $commit_id = backtick("git rev-parse HEAD");
    $describe = backtick("git describe");
    my $commit_created = backtick(qq{git log -1 --pretty="format:%ci"});
    $new_patchnum = "describe: $describe";
    $extra_info = "git_commit_date='$commit_created'";
    if (length $branch && length $remote) {
        # git cherry $remote/$branch | awk 'BEGIN{ORS=","} /\+/ {print $2}' | sed -e 's/,$//'
        my $unpushed_commit_list =
            join ",", map { (split /\s/, $_)[1] }
            grep {/\+/} split /\n/, backtick("git cherry $remote/$branch");
        # git cherry $remote/$branch | awk 'BEGIN{ORS="\t\\\\\n"} /\+/ {print ",\"" $2 "\""}'
        $unpushed_commits =
            join "", map { ',"'.(split /\s/, $_)[1].'"'."\t\\\n" }
            grep {/\+/} split /\n/, backtick("git cherry $remote/$branch");
        if (length $unpushed_commits) {
            $commit_title = "Local Commit:";
            my $ancestor = backtick("git rev-parse $remote/$branch");
            $extra_info = "$extra_info
git_ancestor='$ancestor'
git_unpushed='$unpushed_commit_list'";
        }
    }
    if (length $changed) {
        $changed = 'true';
        $commit_title =  "Derived from:";
        $new_patchnum = "$new_patchnum
status: uncommitted-changes";
    }
    if (not length $commit_title) {
        $commit_title = "Commit id:";
    }
}

my $new_unpushed =<<"EOFTEXT";
/*********************************************************************
* WARNING: unpushed.h is automatically generated by make_patchnum.pl *
*          DO NOT EDIT DIRECTLY - edit make_patchnum.pl instead      *
*********************************************************************/
#define PERL_GIT_UNPUSHED_COMMITS       $unpushed_commits
/*leave-this-comment*/
EOFTEXT

my $new_config =<<"EOFDATA";
#################################################################
# WARNING: lib/Config_git.pl is generated by make_patchnum.pl   #
#          DO NOT EDIT DIRECTLY - edit make_patchnum.pl instead #
#################################################################
\$Config::Git_Data=<<'ENDOFGIT';
git_commit_id='$commit_id'
git_describe='$describe'
git_branch='$branch'
git_uncommitted_changes='$changed'
git_commit_id_title='$commit_title'
$extra_info
ENDOFGIT
EOFDATA

# only update the files if necessary, other build product depends on these files
if (( $existing_patchnum ne $new_patchnum ) || ( $existing_config ne $new_config ) || ( $existing_unpushed ne $new_unpushed )) {
    print "Updating .patchnum and lib/Config_git.pl\n";
    write_file('.patchnum', $new_patchnum);
    write_file('lib/Config_git.pl', $new_config);
    write_file('unpushed.h', $new_unpushed);
}
else {
    print "Reusing .patchnum and lib/Config_git.pl\n"
}

# ex: set ts=4 sts=4 et ft=perl: