package superperm;

import java.util.Date;
import java.util.HashMap;

public class Generator {

	String pattern = "ABCDEF"; //n = length of pattern
	
	HashMap<String, Integer> permLookup = new HashMap<>();
	String[] perms;
	int total, n, nm1, nm2, nm3;
	int[] next;
	long[] chain[], used;
	
	public Generator() {
		makeList();
		initChainData();
	}

	public void makeList() {
		n = pattern.length();
		nm1 = n - 1;
		nm2 = n - 2;
		nm3 = n - 3;
		int[] curPerm = new int[n];
		total = 1;
		for (int i = 0; i < n; i++) {
			curPerm[i] = i + 1;
			total *= (i + 1);
		}
		perms = new String[total];
		String curPattern = pattern;
		for (int i = 0; i < total; i++) {
			int index = nm1;
			perms[i] = curPattern;
			permLookup.put(perms[i], i);
			curPattern = curPattern.substring(1) + curPattern.substring(0, 1);
			while (--curPerm[index] == 0) {
				curPerm[index] = index + 1;
				curPattern = curPattern.substring(1, index) + curPattern.substring(0, 1) + curPattern.substring(index);
				index--;
				if (index < 1) break;
			}
		}
	}

	int cycle(int value, int shift, int cycle) {
		final int rem = value % cycle;
		return ((rem + shift) % cycle - rem + value);
	}

	int rotate(int value) {
		String pattern = perms[value];
		pattern = pattern.substring(1, nm1) + pattern.substring(0, 1) + pattern.substring(nm1);
		return permLookup.get(pattern);
	}
	
	String getLine(int i) {
		return perms[i] + perms[i].substring(0, nm1);
	}
	
	void initChainData() {
		next = new int[total];
		int bitSize = (total / n + 63) / 64; 
		chain = new long[total][bitSize];
		for (int i = 0; i < total; i++) {
			next[i] = rotate(i);
		}
		for (int i = 0; i < total; i++) {
			int value = i, j;
			System.out.print(getLine(value) + "\t");
			for (j = 0; j < nm2; j++) {
				value = next[value];
				System.out.print(getLine(value) + "\t");
				int row = value / n;
				chain[i][row >> 6] |= (1L << (row & 0x3F)); // 6 is 2^6, the number of bits in a long, do not replace with n
			}
//			System.out.println(getLine(next[value]));
		}
	}
	
	public static void main(String[] args) {
		Generator m = new Generator();
		System.out.println("Started at: " + new Date().toString());
		Solver.solve(System.out, m.n, m.next, m.chain);
		System.out.println("Ended at: " + new Date().toString());
	}
}
