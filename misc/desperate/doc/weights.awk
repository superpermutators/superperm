BEGIN {
	FS = ",";
	i = 0;
}

{
	for (i in a) {
		slen = length(a[i]);
		if (slen < $1) {
			a[i] = sprintf("%s%*s%s", a[i], ($1 - slen), "", $2);
			next;
		}
	}
	a[i + 1] = sprintf("%*s%s", $1, "", $2);
}

END {
	for (i in a) {
		print(a[i]);
	}
}
