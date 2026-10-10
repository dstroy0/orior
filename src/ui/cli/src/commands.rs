// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The commands, as commands.json lists them: the window's menus, and the command line's words.
//!
//! A menu is a title and its items, and a menu of jobs names the catalog's groups it lists, split by
//! what each job works on where `split` says so. An item is a label, the command it runs, the keys
//! that run it in the window, the words it takes after its name, and whether the command line runs it
//! itself, in the terminal, or opens the window to run it there. The window reads the same file for
//! the rest of an item: what it needs before it can act, and a label that changes with the scheme.

use serde::Deserialize;

/// The list itself, built into the program so the window and the command line read the same one.
pub const TEXT: &str = include_str!("../commands.json");

#[derive(Deserialize)]
pub struct Commands {
    pub menus: Vec<Menu>,
}

#[derive(Deserialize)]
pub struct Menu {
    pub title: String,
    #[serde(default)]
    pub items: Vec<Entry>,
    #[serde(default)]
    pub groups: Vec<String>,
    #[serde(default)]
    pub split: Option<String>,
}

/// An item of a menu, "-", the line between items, or a label over items of its own, which the window
/// opens beside it and the command line lists with the rest of the menu.
#[derive(Deserialize)]
#[serde(untagged)]
pub enum Entry {
    Line(String),
    Item(Item),
    Group(Group),
}

#[derive(Deserialize)]
pub struct Group {
    pub label: String,
    pub items: Vec<Entry>,
}

#[derive(Deserialize)]
pub struct Item {
    pub label: String,
    pub command: String,
    #[serde(default)]
    pub keys: String,
    #[serde(default)]
    pub args: String,
    #[serde(default)]
    pub console: bool,
}

impl Menu {
    /// The word the command line takes for the menu: its title in lower case.
    pub fn word(&self) -> String {
        self.title.to_lowercase()
    }

    /// Every item of the menu, those of its groups among them, in order.
    pub fn commands(&self) -> impl Iterator<Item = &Item> {
        flat(&self.items).into_iter()
    }

    pub fn command(&self, word: &str) -> Option<&Item> {
        self.commands().find(|item| item.command == word)
    }
}

fn flat(entries: &[Entry]) -> Vec<&Item> {
    entries
        .iter()
        .flat_map(|entry| match entry {
            Entry::Item(item) => vec![item],
            Entry::Group(group) => flat(&group.items),
            Entry::Line(_) => Vec::new(),
        })
        .collect()
}

pub fn read() -> Commands {
    serde_json::from_str(TEXT).expect("commands.json is part of the program")
}

#[cfg(test)]
mod list {
    use std::collections::HashSet;

    #[test]
    fn every_word_names_one_thing() {
        let read = super::read();
        let mut menus = HashSet::new();
        for menu in &read.menus {
            assert!(menus.insert(menu.word()), "two menus are {}", menu.word());
            let mut commands = HashSet::new();
            for item in menu.commands() {
                assert!(commands.insert(item.command.as_str()), "{} has two {}", menu.title, item.command);
                assert!(!item.command.contains(' '), "{} is not one word", item.command);
            }
            if !menu.groups.is_empty() {
                assert!(menu.commands().all(|item| !item.command.contains('/')), "a command of {} reads as a job", menu.title);
            }
        }
    }
}
