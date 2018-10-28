package superperm;

import java.io.PrintStream;

public class Solver {

	static int cycle(int value, int shift, int cycle) {
		final int rem = value % cycle;
		return ((rem + shift) % cycle - rem + value);
	}

	static boolean intersect(long[] bits, long[] otherBits) {
		for (int i = bits.length - 1; i >= 0; i--)
			if ((bits[i] & otherBits[i]) != 0) return true;
		return false;
	}
	
	static void set(long[] bits, long[] otherBits) {
		for (int i = bits.length - 1; i >= 0; i--)
			bits[i] |= otherBits[i];
	}
	
	static void clear(long[] bits, long[] otherBits) {
		for (int i = bits.length - 1; i >= 0; i--)
			bits[i] &= ~otherBits[i];
	}
	
	static void add(int start, int[] next, long[] used, long[][] chain) {
		int value = next[start];
		while (value != start) {
			set(used, chain[value]);
			value = next[value];
		}
	}

	static void remove(int start, int[] next, long[] used, long[][] chain) {
		int value = next[start];
		while (value != start) {
			clear(used, chain[value]);
			value = next[value];
		}
	}

	public static void solve(PrintStream out, int n, int[] next, long[][] chain) {
		int nm1 = n - 1, nm2 = n - 2;
		int total = next.length;
		int bitSize = chain[0].length;
		int groupSize = nm2 * (n - 3);
		int maxInsert = (total / n / nm2 - nm1), insertCur = 0;
		long[] used = new long[bitSize];
		int[] queue = new int[groupSize * (maxInsert + nm2)];
		int queueRead = 0, queueWrite = 0;
		int[] inserts = new int[maxInsert];
		StringBuilder sb = new StringBuilder();
		
		// Add initial cycle
		int bits = nm1 * nm2;
		for (int i = 0; i < (bits >> 6); i++)
			used[i] = ~0;
		used[bits >> 6] = (1 << (bits & 0x3F)) - 1;
		for (int i = 0; i < n * nm1 * nm2; i++) {
			if (!intersect(used, chain[i])) queue[queueWrite++] = i;
		}
		
		queueRead = queueWrite - 1;
		
		while (true) {
			while (queueRead < queueWrite) {
				int value = queue[queueRead++], j;
				if (intersect(used, chain[value])) continue;
				set(used, chain[value]);
				for (j = 0; j < nm2; j++) {
					value = next[value];
					for (int k = 2; k < nm1; k++)
						queue[queueWrite++] = cycle(value, k, n);
				}
				inserts[insertCur++] = queueRead - 1;
				break;
			}
			if (insertCur == 0) break;
			if (insertCur == maxInsert) {
				sb.append("\"");
				for (int i : inserts) {
					sb.append(queue[i] + " ");
				}
				sb.setLength(sb.length() - 1);
				sb.append("\",");
				out.println(sb);
				sb.setLength(0);
			}
			else if (queueRead < queueWrite) continue;
			// Backtrack
			queueWrite -= groupSize;
			queueRead = inserts[--insertCur];
			int value = queue[queueRead++];
			clear(used, chain[value]);
		}
	}
}