pub mod generated;
pub mod codec;
pub mod rle;
pub mod file;
pub mod document;
pub mod cli;

pub use file::{BvgFile, ChunkMeta};
pub use document::Document;

#[derive(Debug)]
pub struct BvgError {
    pub kind: Kind,
    pub msg: String,
}

#[derive(Debug, PartialEq)]
pub enum Kind {
    Format,
    Version,
    Codec,
    Chunk,
}

impl BvgError {
    pub fn format(msg: impl Into<String>) -> Self {
        Self { kind: Kind::Format, msg: msg.into() }
    }
    pub fn version(msg: impl Into<String>) -> Self {
        Self { kind: Kind::Version, msg: msg.into() }
    }
    pub fn codec(msg: impl Into<String>) -> Self {
        Self { kind: Kind::Codec, msg: msg.into() }
    }
    pub fn chunk(msg: impl Into<String>) -> Self {
        Self { kind: Kind::Chunk, msg: msg.into() }
    }
}

impl std::fmt::Display for BvgError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{:?}: {}", self.kind, self.msg)
    }
}

impl std::error::Error for BvgError {}