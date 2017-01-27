//
// Created by VF on 27.01.2017.
//

#include "file_guard.h"
#include "MyServer.h"

void file_guard::get_file(std::string url, response &response) {
    std::size_t pos = url.find("/");
    std::string file_id = url.substr(pos);

    //CREATE TABLE "files" ( `file_id` TEXT UNIQUE, `file_path` TEXT, `limit_type` TEXT DEFAULT 'C', `dl_counter` INTEGER DEFAULT 0, `limit_timestamp` INTEGER, PRIMARY KEY(`file_id`) )
    dlib::statement st2(db, "select * from `files` where `files`.`file_id` = ? COLLATE NOCASE");
    st2.bind(1, file_id);
    st2.exec();

    if (st2.move_next()) {
        std::string file_path, limit_type;
        int dl_counter;
        int64 limit_timestamp;

        file_path = st2.get_column_as_text(1);
        limit_type = st2.get_column_as_text(2);
        dl_counter = st2.get_column_as_int(3);
        limit_timestamp = st2.get_column_as_int64(4);

        switch (limit_type[0]) {
            case 'C': //handle dl counter
                if (dl_counter > 0) {
                    response.response = file_path;
                    response.type = FILE_NAME;
                    dlib::statement st(db, "update `files` set dl_counter = ? where `files`.`file_id` = ? COLLATE NOCASE");
                    st.bind(1, file_id);
                    st.bind(2, dl_counter-1);
                    st.exec();

//                    create trigger fooTrigger
//                    after update of dl_counter on files
//                    for each row when new.dl_counter = 0 begin
//                      delete from foo where dl_counter = 0;
//                    end
                }
                break;
            case 'T': //handle time restriction
                if (get_current_db_time()- limit_timestamp < 0) {
                    response.response = file_path;
                    response.type = FILE_NAME;
                } else {
                    // delete invalid row
                    dlib::statement st(db, "delete from `files` where `file_id` = ?");
                    st.bind(1, file_id);
                    st.exec();
                }
                break;
            default:
                response.type = ERR;
                response.response = "403";
                break;
        }

    } else {
        response.type = ERR;
        response.response = "403";
    }

}

file_guard::file_guard(std::string db_path) : db(db_path) {

}

int64 file_guard::get_current_db_time() {
    dlib::statement stmt(db, "select strftime('%s','now')");
    stmt.exec();
    if (stmt.move_next()) {
        return stmt.get_column_as_int64(0);
    }
    return INT64_MAX;
}
