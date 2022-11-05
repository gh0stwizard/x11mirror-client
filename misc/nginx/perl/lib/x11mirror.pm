package x11mirror;


use strict;
use warnings;
use Encode qw(decode :fallback_all);
use Errno qw(:POSIX);
use Fcntl;
use File::Copy;
use File::Basename;
use File::Path qw(make_path);
use HTTP::MultiPartParser;
use MIME::Base64;
use nginx;


my $file_mode = 0640;
my $dir_mode  = 0750;


sub handler {
    my $r = shift;

    if ($r->request_method eq 'GET') {
        my $file_path = safe_filename($r);

        if (-r $file_path && -f _) {
            $r->header_out('Content-Length', -s _);
            $r->allow_ranges;
            $r->send_http_header;
            $r->sendfile($file_path) unless $r->header_only;
            return OK;
        } else {
            $r->header_out('Content-Type', "text/html");
            $r->send_http_header;
            my $html = <<EOF;
<html>
    <head>
        <title>x11mirror-server</title>
        <meta http-equiv="refresh" content="1">
    </head>
    <body>
        <img alt="pwn2own" src="uploaded.png"></img>
    </body>
</html>
EOF
            $r->print($html);
        }

        return OK;
    }
    elsif ($r->request_method eq 'POST') {
        if (not $r->has_request_body(\&post)) {
            return HTTP_BAD_REQUEST;
        }
    }

    return OK;
}


sub post {
    my $r = shift;

    my $body = $r->request_body;

    if (!$body) {
        my $src_path = $r->request_body_file;
        open my $src, '<', $src_path
            or return system_error($r, "failed to open $src_path");
        read $src, $body, -s $src;
        close $src;
    }

    # dst_dir = root + location, e.g. /tmp/x11mirror
    # where root is /tmp and location /x11mirror
    my $dst_dir = safe_filename($r);
    make_path($dst_dir, {chmod => $dir_mode, error => \my $error});
    if (@$error) {
        return system_error($r, "Cannot create directory $dst_dir");
    }
#    $r->log_error(0, "dst_dir = " . $dst_dir);

    open my $dst, ">", "$dst_dir/uploaded.png";
    binmode $dst;

    my $boundary = (split /boundary=/, $r->header_in('Content-Type'))[1];
#    $r->log_error(0, "parse_body_file boundary: " . $boundary);

    my $filename;
    # this parser can normally handle only base64 data...
    my $parser = HTTP::MultiPartParser->new(
        boundary => $boundary,
        on_header => sub {
            my ($header) = @_;
            foreach my $h (@$header) {
                my ($o, $v) = split ':', $h;
                if ($o =~ m/Content-Disposition/i) {
                    $v =~ m/(?<=filename=)\"(.*)\"/;
                    $filename = $1;
                }
            }
        },
        on_header_as => 'lines',
        on_body => sub {
            my ($chunk, $final) = @_;
            print $dst decode_base64($chunk);
            if ($final) {
                close $dst;
                chmod $dst, $file_mode;
            }
        }
    );
    $parser->parse($body);
    $parser->finish;

    $r->send_http_header;
    $r->print($filename . " uploaded!");

    return OK;
}


sub safe_filename {
    my $r = shift;
    my $filename = decode('UTF-8', $r->filename, FB_DEFAULT | LEAVE_SRC);
    my $uri = decode('UTF-8', $r->uri, FB_DEFAULT | LEAVE_SRC);
    my $safe_uri = $uri =~ s|[^\p{Alnum}/_.-]|_|gr;

    return substr($filename, 0, -length($uri)) . $safe_uri;
}


sub system_error {
    my ($r, $msg) = @_;

    $r->log_error($!, $msg);

    return HTTP_FORBIDDEN if $!{EACCES};
    return HTTP_CONFLICT if $!{EEXIST};
    return HTTP_INTERNAL_SERVER_ERROR;
}


1;
__END__
