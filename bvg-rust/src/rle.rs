pub fn encode(data: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(data.len());
    let mut i = 0;
    let n = data.len();
    while i < n {
        let b = data[i];
        let mut j = i + 1;
        while j < n && data[j] == b && (j - i) < 130 {
            j += 1;
        }
        let run = j - i;
        if run >= 3 {
            out.push(0x80 | (run as u8 - 3));
            out.push(b);
            i = j;
        } else {
            let start = i;
            i += 1;
            while i < n && (i - start) < 128 {
                if i + 2 < n && data[i] == data[i + 1] && data[i] == data[i + 2] {
                    break;
                }
                i += 1;
            }
            let lit = i - start;
            out.push((lit - 1) as u8);
            out.extend_from_slice(&data[start..i]);
        }
    }
    out
}

pub fn decode(data: &[u8]) -> Result<Vec<u8>, crate::BvgError> {
    let mut out = Vec::with_capacity(data.len() * 2);
    let mut i = 0;
    let n = data.len();
    while i < n {
        let h = data[i]; i += 1;
        if h & 0x80 != 0 {
            let run = ((h & 0x7F) as usize) + 3;
            if i >= n {
                return Err(crate::BvgError::codec("RLE: truncated run"));
            }
            let b = data[i]; i += 1;
            out.extend(std::iter::repeat(b).take(run));
        } else {
            let lit = (h as usize) + 1;
            if i + lit > n {
                return Err(crate::BvgError::codec("RLE: truncated literal"));
            }
            out.extend_from_slice(&data[i..i + lit]);
            i += lit;
        }
    }
    Ok(out)
}