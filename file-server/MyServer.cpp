#include "MyServer.h"

void MyServer::stream_http_response(std::ostream &out, outgoing_things outgoing, std::string &filename) {
    dlib::file file(filename);
    uint64 filesize = file.size();

    outgoing.headers["Content-Type"] = "application/octet-stream";
    outgoing.headers["Content-Length"] = std::to_string(filesize);
    outgoing.headers["Content-Transfer-Encoding"] = "binary";
    outgoing.headers["Content-Disposition"] = "attachment; filename=\"" + file.name() + "\"";


    out << "HTTP/1.0 " << outgoing.http_return << " " << outgoing.http_return_status << "\r\n";

    // Set any new headers
    for (key_value_map_ci::const_iterator ci = outgoing.headers.begin(); ci != outgoing.headers.end(); ++ci) {
        out << ci->first << ": " << ci->second << "\r\n";
    }
    out << "\r\n";

    std::ifstream in(file.full_name(), std::ifstream::binary);
    uint64 current = 0;

    while (current < filesize && out.good()) {
        in.read(buffer.get(), chunk_size);
        std::streamsize bytes = in.gcount();
        out.write(buffer.get(), bytes);
        current += bytes;
        t.sleep();
    }

    in.close();
}

void
MyServer::on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip, const std::string &local_ip,
                     unsigned short foreign_port, unsigned short local_port, uint64) {
    try {
        incoming_things incoming(foreign_ip, local_ip, foreign_port, local_port);
        outgoing_things outgoing;

        parse_http_request(in, incoming, get_max_content_length());
        read_body(in, incoming);

        std::string admin_prefix("/admin");
        //incoming.path[0:prefix.len()] == prefix
        if (incoming.path.compare(0, admin_prefix.size(), admin_prefix) == 0) {
            std::string response = admin.on_request(incoming, outgoing);
            write_http_response(out, outgoing, response);
        } else {
            response response;
            on_request(incoming, outgoing, response);
            if (response.type == STRING) {
                write_http_response(out, outgoing, response.response);
            } else if (response.type == FILE_NAME) {
                stream_http_response(out, outgoing, response.response);
            } else if (response.type == FILE_NOT_AVAILABLE) {
                outgoing.http_return = 404;
                outgoing.http_return_status = response.response;
                write_http_response(out, outgoing, response.response);
            }
        }
    }
    catch (std::exception &e) {
        write_http_response(out, e);
    }
}

void MyServer::on_request(const incoming_things &incoming, outgoing_things &outgoing, response &response) {
    if (incoming.path != "/") {
        fileguard.get_file(incoming.path, response);
        return;
    }

    response.type = STRING;
    response.response = "<html> <body> nothing to see here </body> </html>";
}

MyServer::MyServer(server_config &config) : chunk_size(config.chunk_size),
                                            buffer(new char[chunk_size]),
                                            t(8, chunk_size),
                                            db(config.db_path),
                                            fileguard(db),
                                            debug(config.debug),
                                            root_dir(config.root_path),
                                            admin(db, root_dir) {}
