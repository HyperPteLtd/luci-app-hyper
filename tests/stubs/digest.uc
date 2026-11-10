// Test stub: deterministic 32-hex-digit hash (not MD5, only stable and spread out)
export function md5(s) {
	let a = 0x811c9dc5, b = 0x1505, out = '';
	for (let round = 0; round < 4; round++) {
		for (let i = 0; i < length(s); i++) {
			a = ((a ^ ord(s, i)) * 16777619 + round) & 0xffffffff;
			b = ((b * 33) ^ ord(s, i) ^ a) & 0xffffffff;
		}
		out += sprintf('%08x', (a ^ b) & 0xffffffff);
	}
	return out;
};
