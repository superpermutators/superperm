package superperm;

import java.util.Date;

public class Main {

	int cycle(int value, int shift, int cycle) {
		final int rem = value % cycle;
		return ((rem + shift) % cycle - rem + value);
	}

	int rotate(int value) {
		int shift, cycle = 30;
		int m1 = value % 6;
		switch (m1) {
		case 0:
			shift = 6;
			break;
		case 5:
			shift = 20;
			break;
		default:
			cycle = 120;
			int m2 = (value / 6 + m1) % 5;
			switch (m2) {
			case 0:
				shift = 25;
				break;
			case 4:
				shift = 97;
				break;
			default:
				cycle = 360;
				int m3 = (value / 30 + m2) % 4;
				switch (m3) {
				case 0:
					shift = 91;
					break;
				case 3:
					shift = 271;
					break;
				default:
					cycle = 720;
					shift = (value / 120 + m3) % 3 * 120 + 241;
				}
			}
		}
		return cycle(value, shift, cycle);
	}

	void findChain() {
		int next[] = new int[720];
		long[] chain1 = new long[720], chain2 = new long[720];
		long used1, used2;
		int queue[] = new int[324];
		int queueMax = 0;
		int queueCur = 23;
		
		for (int i = 0; i < 720; i++) {
			next[i] = rotate(i);
		}
		for (int i = 0; i < 720; i++) {
			int value = i, j;
			for (j = 0; j < 4; j++) {
				value = next[value];
				int row = value / 6;
				if (row >= 64)
					chain2[i] |= (1L << (row - 64));
				else
					chain1[i] |= (1L << row);
			}
		}
		used1 = 0xFFFFF;
		used2 = 0;
		for (int i = 0; i < 120; i++) {
			if ((used1 & chain1[i]) == 0 && (used2 & chain2[i]) == 0)
				queue[queueMax++] = i;
		}
		for (int i = 0; i < queueMax; i++)
			System.out.print(queue[i] + " ");
		System.out.println();

		int inserts[] = new int[25];
		int insertCur = 0;
		while (true) {
			while (queueCur < queueMax) {
				int value = queue[queueCur++], j;
				if ((used1 & chain1[value]) != 0 || (used2 & chain2[value]) != 0)
					continue;
				used1 |= chain1[value];
				used2 |= chain2[value];
				for (j = 0; j < 4; j++) {
					value = next[value];
					for (int k = 2; k < 5; k++)
						queue[queueMax++] = cycle(value, k, 6);
				}
				inserts[insertCur++] = queueCur - 1;
				break;
			}
			if (insertCur == 25) {
				// Print and backtrack
				for (int i : inserts) {
					System.out.print(queue[i] + " ");
				}
				System.out.println();
				queueMax -= 12;
				queueCur = inserts[--insertCur];
				int value = queue[queueCur++];
				used1 &= ~chain1[value];
				used2 &= ~chain2[value];
			}
			if (queueCur >= queueMax) {
				// Backtrack
				queueMax -= 12;
				queueCur = inserts[--insertCur];
				int value = queue[queueCur++];
				used1 &= ~chain1[value];
				used2 &= ~chain2[value];
				if (insertCur == 0) break;
			}
		}
		for (int i = 0; i < 720; i++) {
			System.out.print(i);
			int j = i;
			for (int k = 0; k < 5; k++) {
				j = next[j];
				System.out.print("\t" + j);
			}
			System.out.println();
		}
	}

	public static void main(String[] args) {
		Main m = new Main();
		System.out.println("Started at: " + new Date().toString());
		m.findChain();
		System.out.println("Ended at: " + new Date().toString());
	}
}
