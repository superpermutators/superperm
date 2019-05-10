:: Simple wrapper that works like curl for windows users
:: Note that it is fragile - it assumes the URL is in the third
:: parameter and ignores the rest. This works with how URL_UTILITY
:: is currently designed.
@powershell -Command "(new-object net.webclient).DownloadString(\"%3\"")"