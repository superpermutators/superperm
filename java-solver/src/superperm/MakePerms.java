package superperm;

import java.io.IOException;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class MakePerms {
	String[]	perms		= new String[720];
	String		pattern		= "ABCDEF";
	String		pattern2	= "abcdefa";
	String		pattern3	= "abcdefbacdefbcadefbcdaefbcdeafbcdefa";
	String		target		= "123456";

	public void makeList() {
		int[] curPerm = new int[pattern.length()];
		int total = 1;
		for (int i = 0; i < curPerm.length; i++) {
			curPerm[i] = i + 1;
			total *= (i + 1);
		}
		perms = new String[total];
		String curPattern = pattern;
		for (int i = 0; i < total; i++) {
			int index = pattern.length() - 1;
			perms[i] = curPattern;
			curPattern = curPattern.substring(1) + curPattern.substring(0, 1);
			while (--curPerm[index] == 0) {
				curPerm[index] = index + 1;
				curPattern = curPattern.substring(1, index) + curPattern.substring(0, 1) + curPattern.substring(index);
				index--;
				if (index < 1) break;
			}
		}
	}

	public String replacePattern(String src, String pattern) {
		for (int i = 0; i < pattern.length(); i++) {
			src = src.replace(src.charAt(i), pattern.charAt(i));
		}
		return src;
	}

	public String greedyStart() {
		String s = pattern;
		int count = pattern.length() * (pattern.length() - 1) * (pattern.length() - 2);
		for (int i = 0; i < count; i++) {
			int index = s.lastIndexOf(perms[i].charAt(0));
			s = s.substring(0, index) + perms[i];
		}
		return s;
	}

	String[] inserts = {
			"118 595 592 474 523 571 679 616 438 529 390 481 577 204 366 195 216 168 164 620 260 257 306 314 304" };

	public boolean checkPattern(String s) {
		for (String p : perms) {
			if (!s.contains(p)) return false;
		}
		return true;
	}

	public void outputIfOk(String s) {
		if (checkPattern(s)) {
			String actual = replacePattern(s, target) + "\n";
			try {
				MessageDigest md5 = MessageDigest.getInstance("MD5");
				md5.update(StandardCharsets.UTF_8.encode(actual));
				String hash = String.format("%032x", new BigInteger(1, md5.digest()));
				FileChannel fc = FileChannel.open(Paths.get("872." + hash.substring(0, 7) + ".txt"), StandardOpenOption.WRITE, StandardOpenOption.CREATE);
				fc.write(ByteBuffer.wrap(actual.getBytes()));
				fc.close();
			} catch (NoSuchAlgorithmException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			System.out.println(replacePattern(s, target));
		}
	}

	public void makePatterns() {
		int size = pattern.length() - 1;
		for (String insert : inserts) {
			String greedy = greedyStart();
			String[] positions = insert.split(" ");
			for (String p : positions) {
				int i = Integer.valueOf(p);
				String adjPerm = perms[i].substring(size) + perms[i].substring(0, size);
				String toReplace = replacePattern(pattern2, adjPerm);
				String replaceWith = replacePattern(pattern3, adjPerm);
				greedy = greedy.replaceAll(toReplace, replaceWith);
			}
			/* BEGIN LENGTH 6 SPECIFIC CODE */
			int part2 = greedy.indexOf("BCDAEF");
			int part3 = greedy.indexOf("CDABEF");
			int part4 = greedy.indexOf("DABCEF");
			String sp1 = greedy.substring(3, part2 + 3), sp1r = new StringBuilder(sp1).reverse().toString();
			String sp2 = greedy.substring(part2 + 3, part3 + 3), sp2r = new StringBuilder(sp2).reverse().toString();
			String sp3 = greedy.substring(part3 + 3, part4 + 3), sp3r = new StringBuilder(sp3).reverse().toString();
			String sp4 = greedy.substring(part4 + 3), sp4r = new StringBuilder(sp4).reverse().toString();
			outputIfOk("ABC" + sp1 + sp2 + sp3 + sp4);
			outputIfOk("BCD" + sp2 + sp3 + sp4 + sp1);
			outputIfOk("CDA" + sp3 + sp4 + sp1 + sp2);
			outputIfOk("DAB" + sp4 + sp1 + sp2 + sp3);
			outputIfOk(sp4r + sp3r + sp2r + sp1r + "CBA");
			outputIfOk(sp1r + sp4r + sp3r + sp2r + "DCB");
			outputIfOk(sp2r + sp1r + sp4r + sp3r + "ADC");
			outputIfOk(sp3r + sp2r + sp1r + sp4r + "BAD");
			/* END LENGTH 6 SPECIFIC CODE */
		}
	}

	public static void main(String[] args) {
		MakePerms mp = new MakePerms();
		mp.makeList();
		mp.makePatterns();
	}
}
