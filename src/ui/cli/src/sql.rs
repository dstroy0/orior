// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! SQL as the database window reads it before it runs: the statements of a text, the one a place in
//! it stands in, whether one writes, an UPDATE or a DELETE that reaches every row of its table, and
//! the object a statement that changes the schema names.
//!
//! The text is read as each database writes it: PostgreSQL's dollar quotes, its `E''` strings and its
//! nested comments; MySQL's `#` comments, its backslash escapes and its backquoted names; SQLite's
//! names in brackets. A semicolon inside a string, a quoted name, a comment, a dollar quote or the
//! BEGIN and END of a trigger's or a routine's body does not end a statement.

/// The SQL each database writes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Dialect {
    Postgres,
    Mysql,
    Sqlite,
}

impl Dialect {
    pub fn of(name: &str) -> Dialect {
        match name {
            "mysql" | "mariadb" => Dialect::Mysql,
            "sqlite" => Dialect::Sqlite,
            _ => Dialect::Postgres,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Kind {
    Word,
    Name,
    Text,
    Number,
    Mark,
    Comment,
    Space,
}

#[derive(Clone, Copy, Debug)]
struct Token {
    kind: Kind,
    start: usize,
    end: usize,
}

/// The tokens of `text`, every byte in one.
fn tokens(text: &str, dialect: Dialect) -> Vec<Token> {
    let bytes = text.as_bytes();
    let mut out = Vec::new();
    let mut at = 0;
    while at < bytes.len() {
        let start = at;
        let byte = bytes[at];
        let next = bytes.get(at + 1).copied();
        let kind = if byte.is_ascii_whitespace() {
            while at < bytes.len() && bytes[at].is_ascii_whitespace() {
                at += 1;
            }
            Kind::Space
        } else if (byte == b'-' && next == Some(b'-')) || (byte == b'#' && dialect == Dialect::Mysql) {
            while at < bytes.len() && bytes[at] != b'\n' {
                at += 1;
            }
            Kind::Comment
        } else if byte == b'/' && next == Some(b'*') {
            let mut depth = 0;
            while at < bytes.len() {
                if bytes[at] == b'/' && bytes.get(at + 1) == Some(&b'*') {
                    depth += if dialect == Dialect::Postgres || depth == 0 { 1 } else { 0 };
                    at += 2;
                } else if bytes[at] == b'*' && bytes.get(at + 1) == Some(&b'/') {
                    depth -= 1;
                    at += 2;
                    if depth == 0 {
                        break;
                    }
                } else {
                    at += 1;
                }
            }
            Kind::Comment
        } else if byte == b'\'' || ((byte == b'E' || byte == b'e') && next == Some(b'\'') && dialect == Dialect::Postgres) {
            let escapes = dialect == Dialect::Mysql || byte != b'\'';
            at += if byte == b'\'' { 1 } else { 2 };
            quoted(bytes, &mut at, b'\'', escapes);
            Kind::Text
        } else if byte == b'"' {
            at += 1;
            quoted(bytes, &mut at, b'"', dialect == Dialect::Mysql);
            if dialect == Dialect::Mysql {
                Kind::Text
            } else {
                Kind::Name
            }
        } else if byte == b'`' && dialect != Dialect::Postgres {
            at += 1;
            quoted(bytes, &mut at, b'`', false);
            Kind::Name
        } else if byte == b'[' && dialect == Dialect::Sqlite {
            while at < bytes.len() && bytes[at] != b']' {
                at += 1;
            }
            at = (at + 1).min(bytes.len());
            Kind::Name
        } else if byte == b'$' && dialect == Dialect::Postgres && dollar_tag(bytes, at).is_some() {
            let tag = dollar_tag(bytes, at).unwrap_or_default();
            at += tag.len();
            match find(&bytes[at..], tag.as_bytes()) {
                Some(found) => at += found + tag.len(),
                None => at = bytes.len(),
            }
            Kind::Text
        } else if byte.is_ascii_alphabetic() || byte == b'_' || byte >= 0x80 {
            while at < bytes.len() && (bytes[at].is_ascii_alphanumeric() || bytes[at] == b'_' || bytes[at] == b'$' || bytes[at] >= 0x80) {
                at += 1;
            }
            Kind::Word
        } else if byte.is_ascii_digit() {
            while at < bytes.len() && (bytes[at].is_ascii_alphanumeric() || bytes[at] == b'.') {
                at += 1;
            }
            Kind::Number
        } else {
            at += 1;
            Kind::Mark
        };
        out.push(Token { kind, start, end: at });
    }
    out
}

/// Moves `at` past a quoted run that began just before it and ends at `close`, a doubled `close`
/// standing for itself, and a backslash escaping the byte after it where `escapes` is set.
fn quoted(bytes: &[u8], at: &mut usize, close: u8, escapes: bool) {
    while *at < bytes.len() {
        if escapes && bytes[*at] == b'\\' {
            *at += 2;
        } else if bytes[*at] == close {
            *at += 1;
            if bytes.get(*at) == Some(&close) {
                *at += 1;
            } else {
                return;
            }
        } else {
            *at += 1;
        }
    }
    *at = bytes.len();
}

/// The dollar quote's tag that starts at `at`, `$$` or `$name$`.
fn dollar_tag(bytes: &[u8], at: usize) -> Option<String> {
    let mut end = at + 1;
    while end < bytes.len() && (bytes[end].is_ascii_alphanumeric() || bytes[end] == b'_') {
        end += 1;
    }
    if bytes.get(end) == Some(&b'$') && !bytes.get(at + 1).is_some_and(u8::is_ascii_digit) {
        return Some(String::from_utf8_lossy(&bytes[at..=end]).into_owned());
    }
    None
}

fn find(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack.windows(needle.len()).position(|window| window == needle)
}

/// A statement of a text: where it starts and ends, past the spaces and comments around it and
/// before its semicolon.
#[derive(Clone, Copy, Debug, PartialEq, Eq, serde::Serialize)]
pub struct Span {
    pub start: usize,
    pub end: usize,
}

/// Each statement of `text`, in order.
pub fn statements(text: &str, dialect: Dialect) -> Vec<Span> {
    let all = tokens(text, dialect);
    let mut out = Vec::new();
    let mut first: Option<usize> = None;
    let mut last = 0;
    let mut words: Vec<String> = Vec::new();
    // The depth of BEGIN and CASE in a body, and an END not yet known to close one of them: END IF,
    // END LOOP, END WHILE and END REPEAT close blocks that are not counted.
    let mut depth = 0i32;
    let mut ending = false;
    for token in &all {
        match token.kind {
            Kind::Space | Kind::Comment => continue,
            Kind::Mark if &text[token.start..token.end] == ";" => {
                if ending {
                    depth -= 1;
                    ending = false;
                }
                if depth <= 0 {
                    if let Some(start) = first.take() {
                        out.push(Span { start, end: last });
                    }
                    words.clear();
                    depth = 0;
                    continue;
                }
            }
            Kind::Word => {
                let word = text[token.start..token.end].to_ascii_lowercase();
                if has_body(&words) {
                    let closed = std::mem::take(&mut ending);
                    if closed && !matches!(word.as_str(), "if" | "loop" | "while" | "repeat") {
                        depth -= 1;
                    }
                    if !(closed && matches!(word.as_str(), "if" | "loop" | "while" | "repeat" | "case")) {
                        match word.as_str() {
                            "begin" | "case" => depth += 1,
                            "end" => ending = true,
                            _ => {}
                        }
                    }
                }
                if words.len() < 64 {
                    words.push(word);
                }
            }
            _ => {
                if std::mem::take(&mut ending) {
                    depth -= 1;
                }
            }
        }
        first.get_or_insert(token.start);
        last = token.end;
    }
    if let Some(start) = first {
        out.push(Span { start, end: last });
    }
    out
}

/// Whether a statement that begins with `words` holds a body of statements between BEGIN and END:
/// a trigger, a routine or an event. A body PostgreSQL holds in a dollar quote is one string.
fn has_body(words: &[String]) -> bool {
    words.first().map(String::as_str) == Some("create") && words.iter().take(8).any(|word| matches!(word.as_str(), "trigger" | "procedure" | "function" | "event"))
}

/// The statement of `text` the place `at` stands in: within it, at its semicolon, or after it on
/// its last line with nothing but spaces and a comment between. None where the place stands in no
/// statement, on a blank line or in a comment of its own.
pub fn statement_at(text: &str, at: usize, dialect: Dialect) -> Option<Span> {
    let all = statements(text, dialect);
    for span in &all {
        if at >= span.start && at <= span.end {
            return Some(*span);
        }
    }
    for span in all.iter().rev() {
        if span.end <= at {
            let between = &text[span.end..at.min(text.len())];
            let between = between.strip_prefix(';').unwrap_or(between);
            let line = between.split('\n').next().unwrap_or("");
            if !between.contains('\n') && (line.trim().is_empty() || line.trim_start().starts_with("--") || line.trim_start().starts_with('#')) {
                return Some(*span);
            }
            return None;
        }
    }
    None
}

/// What a statement does, by its first word, and where it is EXPLAIN ANALYZE, by what it explains.
#[derive(Clone, Copy, Debug, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Does {
    /// Reads and changes nothing.
    Reads,
    /// Changes rows.
    Writes,
    /// Changes the schema.
    Shapes,
    /// Begins, commits or rolls back a transaction.
    Transaction,
    /// Anything else: a setting, a database chosen.
    Other,
}

/// The words of a statement outside its strings, names and comments, in lower case, each with the
/// depth of brackets it stands at.
fn words_of(statement: &str, dialect: Dialect) -> Vec<(String, i32, usize, usize)> {
    let mut depth = 0;
    let mut out = Vec::new();
    for token in tokens(statement, dialect) {
        let text = &statement[token.start..token.end];
        match token.kind {
            Kind::Mark if text == "(" => depth += 1,
            Kind::Mark if text == ")" => depth -= 1,
            Kind::Word => out.push((text.to_ascii_lowercase(), depth, token.start, token.end)),
            _ => {}
        }
    }
    out
}

/// What `statement` does.
pub fn does(statement: &str, dialect: Dialect) -> Does {
    let words = words_of(statement, dialect);
    let first = words.first().map(|(word, ..)| word.as_str()).unwrap_or("");
    let writes = |words: &[(String, i32, usize, usize)]| words.iter().any(|(word, ..)| matches!(word.as_str(), "insert" | "update" | "delete" | "merge" | "replace" | "upsert"));
    match first {
        "select" => {
            // SELECT ... INTO a new table makes it, in PostgreSQL; in MySQL it writes a file or a
            // variable.
            let into = words.iter().any(|(word, depth, ..)| word == "into" && *depth == 0);
            if into && dialect == Dialect::Postgres {
                Does::Shapes
            } else {
                Does::Reads
            }
        }
        "with" => {
            if writes(&words) {
                Does::Writes
            } else {
                Does::Reads
            }
        }
        "show" | "describe" | "desc" | "values" | "table" | "help" => Does::Reads,
        "explain" => {
            if words.iter().take(4).any(|(word, ..)| word == "analyze" || word == "analyse") && writes(&words) {
                Does::Writes
            } else {
                Does::Reads
            }
        }
        "pragma" => {
            if statement.contains('=') {
                Does::Other
            } else {
                Does::Reads
            }
        }
        "insert" | "update" | "delete" | "merge" | "replace" | "upsert" | "copy" | "load" | "call" | "do" | "execute" | "exec" => Does::Writes,
        "create" | "alter" | "drop" | "truncate" | "rename" | "comment" | "grant" | "revoke" | "reindex" | "cluster" | "refresh" | "attach" | "detach" | "vacuum" | "import" => Does::Shapes,
        "begin" | "start" | "commit" | "rollback" | "savepoint" | "release" | "end" | "abort" | "xa" => Does::Transaction,
        _ => Does::Other,
    }
}

/// Whether running `statement` changes anything the database holds.
pub fn changes(statement: &str, dialect: Dialect) -> bool {
    matches!(does(statement, dialect), Does::Writes | Does::Shapes)
}

/// The query that counts the rows `statement` reaches, where it is an UPDATE or a DELETE with no
/// WHERE at its own level, or a TRUNCATE: every row of what it names.
pub fn unbounded(statement: &str, dialect: Dialect) -> Option<String> {
    let words = words_of(statement, dialect);
    let level: Vec<&(String, i32, usize, usize)> = words.iter().filter(|(_, depth, ..)| *depth == 0).collect();
    let first = level.first()?;
    let has = |name: &str| level.iter().any(|(word, ..)| word == name);
    let after = |word: &(String, i32, usize, usize)| word.3;
    let before = |word: &(String, i32, usize, usize)| word.2;
    let ends = |from: usize, stops: &[&str]| level.iter().filter(|(word, _, start, _)| *start > from && stops.contains(&word.as_str())).map(|word| before(word)).min().unwrap_or(statement.len());
    let source = match first.0.as_str() {
        "update" if !has("where") => {
            let set = level.iter().find(|(word, ..)| word == "set")?;
            statement[after(first)..before(set)].trim().to_string()
        }
        "delete" if !has("where") => {
            let from = level.iter().find(|(word, ..)| word == "from")?;
            let end = ends(after(from), &["using", "returning", "order", "limit"]);
            let names = statement[after(from)..end].trim();
            match level.iter().find(|(word, ..)| word == "using") {
                Some(using) => format!("{names}, {}", statement[after(using)..ends(after(using), &["returning"])].trim()),
                None => names.to_string(),
            }
        }
        "truncate" => {
            let start = level.get(1).filter(|(word, ..)| word == "table").map(|word| after(word)).unwrap_or(after(first));
            let end = ends(start, &["restart", "continue", "cascade", "restrict"]);
            let names = statement[start..end].trim();
            if names.contains(',') {
                // Each table a TRUNCATE names is counted alone and the counts added.
                return Some(format!("SELECT {}", names.split(',').map(|name| format!("(SELECT count(*) FROM {})", name.trim())).collect::<Vec<_>>().join(" + ")));
            }
            names.to_string()
        }
        _ => return None,
    };
    if source.is_empty() {
        return None;
    }
    Some(format!("SELECT count(*) FROM {source}"))
}

/// The object a statement that changes the schema names: what it does to it, its kind and its name
/// as written, and, for an index or a trigger, the table it is on.
#[derive(Clone, Debug, PartialEq, Eq, serde::Serialize)]
pub struct Shaped {
    pub verb: String,
    pub kind: String,
    pub name: String,
    pub on: Option<String>,
}

/// The object `statement` makes, changes or drops, where it is a statement that shapes one.
pub fn shaped(statement: &str, dialect: Dialect) -> Option<Shaped> {
    let all = tokens(statement, dialect);
    let words: Vec<(Kind, String)> = all.iter().filter(|token| !matches!(token.kind, Kind::Space | Kind::Comment)).map(|token| (token.kind, statement[token.start..token.end].to_string())).collect();
    let lower = |at: usize| words.get(at).map(|(_, word)| word.to_ascii_lowercase()).unwrap_or_default();
    let verb = lower(0);
    if !matches!(verb.as_str(), "create" | "alter" | "drop" | "truncate" | "rename" | "comment") {
        return None;
    }
    let mut at = 1;
    if verb == "create" && lower(1) == "or" && lower(2) == "replace" {
        at = 3;
    }
    let skip = ["temp", "temporary", "unique", "materialized", "unlogged", "global", "local", "foreign", "virtual", "recursive", "definer", "algorithm", "sql", "security"];
    while skip.contains(&lower(at).as_str()) {
        at += 1;
    }
    let mut kind = lower(at);
    if verb == "truncate" && kind != "table" {
        kind = "table".to_string();
    } else {
        at += 1;
    }
    if verb == "comment" {
        return None;
    }
    while matches!(lower(at).as_str(), "if" | "not" | "exists" | "concurrently") {
        at += 1;
    }
    let mut name = String::new();
    while at < words.len() {
        let (word_kind, word) = &words[at];
        let joined = matches!(word_kind, Kind::Word | Kind::Name) || word == ".";
        if !joined || (!name.is_empty() && !name.ends_with('.') && word != ".") {
            break;
        }
        name.push_str(word);
        at += 1;
    }
    if name.is_empty() {
        return None;
    }
    let on = words.iter().position(|(_, word)| word.eq_ignore_ascii_case("on")).filter(|_| matches!(kind.as_str(), "index" | "trigger")).and_then(|found| {
        let mut table = String::new();
        let mut at = found + 1;
        if lower(at) == "only" {
            at += 1;
        }
        while at < words.len() {
            let (word_kind, word) = &words[at];
            if !(matches!(word_kind, Kind::Word | Kind::Name) || word == ".") || (!table.is_empty() && !table.ends_with('.') && word != ".") {
                break;
            }
            table.push_str(word);
            at += 1;
        }
        (!table.is_empty()).then_some(table)
    });
    Some(Shaped { verb, kind, name, on })
}

#[cfg(test)]
mod reading {
    use super::*;

    fn texts(text: &str, dialect: Dialect) -> Vec<&str> {
        statements(text, dialect).iter().map(|span| &text[span.start..span.end]).collect()
    }

    #[test]
    fn a_semicolon_in_a_string_a_comment_or_a_dollar_quote_ends_no_statement() {
        let text = "select ';' -- a; comment\n; /* x; /* y; */ z; */ select $f$ a; b $f$ ; select E'it\\'s; ok'";
        assert_eq!(texts(text, Dialect::Postgres), vec!["select ';'", "select $f$ a; b $f$", "select E'it\\'s; ok'"]);
        let mysql = "select 'it\\'s; ok' # a; comment\n; select `a;b` from t";
        assert_eq!(texts(mysql, Dialect::Mysql), vec!["select 'it\\'s; ok'", "select `a;b` from t"]);
        let sqlite = "select [a;b] from t; select \"c;d\"";
        assert_eq!(texts(sqlite, Dialect::Sqlite), vec!["select [a;b] from t", "select \"c;d\""]);
    }

    #[test]
    fn a_trigger_s_body_is_one_statement() {
        let text = "create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end; select 1";
        assert_eq!(texts(text, Dialect::Sqlite), vec!["create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end", "select 1"]);
        let routine = "CREATE PROCEDURE p() BEGIN IF 1 THEN SELECT 1; END IF; SELECT CASE WHEN 1 THEN 2 END; END; SELECT 2";
        assert_eq!(texts(routine, Dialect::Mysql).len(), 2);
    }

    #[test]
    fn the_statement_at_a_place_is_the_one_it_stands_in_and_none_on_a_blank_line() {
        let text = "select 1;\n\n-- note\nselect 2; -- after\nselect\n  3";
        let at = |needle: &str| text.find(needle).unwrap();
        let shown = |place: usize| statement_at(text, place, Dialect::Postgres).map(|span| &text[span.start..span.end]);
        assert_eq!(shown(at("1")), Some("select 1"));
        assert_eq!(shown(at("1;") + 2), Some("select 1"));
        assert_eq!(shown(at("\n\n") + 1), None);
        assert_eq!(shown(at("note")), None);
        assert_eq!(shown(at("after")), Some("select 2"));
        assert_eq!(shown(text.len()), Some("select\n  3"));
    }

    #[test]
    fn what_a_statement_does_is_read_past_its_comments() {
        assert_eq!(does("/* x */ SELECT * FROM t", Dialect::Postgres), Does::Reads);
        assert_eq!(does("with gone as (delete from t returning *) select * from gone", Dialect::Postgres), Does::Writes);
        assert_eq!(does("explain analyze delete from t", Dialect::Postgres), Does::Writes);
        assert_eq!(does("explain delete from t", Dialect::Postgres), Does::Reads);
        assert_eq!(does("select * into copy from t", Dialect::Postgres), Does::Shapes);
        assert_eq!(does("drop table t", Dialect::Mysql), Does::Shapes);
        assert_eq!(does("begin", Dialect::Sqlite), Does::Transaction);
        assert_eq!(does("pragma table_info(t)", Dialect::Sqlite), Does::Reads);
        assert_eq!(does("set search_path = a", Dialect::Postgres), Does::Other);
    }

    #[test]
    fn a_write_with_no_where_is_counted_whole() {
        assert_eq!(unbounded("update only t as x set n = 1", Dialect::Postgres).as_deref(), Some("SELECT count(*) FROM only t as x"));
        assert_eq!(unbounded("update t set n = (select max(n) from u where u.k = 1)", Dialect::Postgres).as_deref(), Some("SELECT count(*) FROM t"));
        assert_eq!(unbounded("update t set n = 1 where k = 2", Dialect::Postgres), None);
        assert_eq!(unbounded("delete from s.t using u returning *", Dialect::Postgres).as_deref(), Some("SELECT count(*) FROM s.t, u"));
        assert_eq!(unbounded("DELETE FROM t", Dialect::Mysql).as_deref(), Some("SELECT count(*) FROM t"));
        assert_eq!(unbounded("update a join b on a.k = b.k set a.n = b.n", Dialect::Mysql).as_deref(), Some("SELECT count(*) FROM a join b on a.k = b.k"));
        assert_eq!(unbounded("truncate table t restart identity", Dialect::Postgres).as_deref(), Some("SELECT count(*) FROM t"));
        assert_eq!(unbounded("truncate a, b", Dialect::Postgres).as_deref(), Some("SELECT (SELECT count(*) FROM a) + (SELECT count(*) FROM b)"));
        assert_eq!(unbounded("delete from t where 'where' = ''", Dialect::Postgres), None);
        assert_eq!(unbounded("select 1", Dialect::Postgres), None);
    }

    #[test]
    fn a_statement_that_shapes_the_schema_names_its_object() {
        let made = shaped("CREATE TABLE IF NOT EXISTS public.\"Orders\" (id int)", Dialect::Postgres).unwrap();
        assert_eq!((made.verb.as_str(), made.kind.as_str(), made.name.as_str()), ("create", "table", "public.\"Orders\""));
        let index = shaped("create unique index concurrently ix on only s.t (a)", Dialect::Postgres).unwrap();
        assert_eq!((index.kind.as_str(), index.name.as_str(), index.on.as_deref()), ("index", "ix", Some("s.t")));
        let view = shaped("create or replace materialized view v as select 1", Dialect::Postgres).unwrap();
        assert_eq!((view.kind.as_str(), view.name.as_str()), ("view", "v"));
        let gone = shaped("drop table if exists `db`.`t`", Dialect::Mysql).unwrap();
        assert_eq!((gone.verb.as_str(), gone.name.as_str()), ("drop", "`db`.`t`"));
        assert_eq!(shaped("truncate t", Dialect::Postgres).map(|one| one.name), Some("t".to_string()));
        assert_eq!(shaped("select 1", Dialect::Postgres), None);
    }
}
