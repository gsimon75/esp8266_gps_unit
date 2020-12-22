server {
	server_name client.wodeewa.com;
	listen 443 ssl;
	ssl_certificate /etc/nginx/pki/STAR_wodeewa_com.chained.crt;
	ssl_certificate_key /etc/nginx/pki/wodeewa_com.key;

	ssl_prefer_server_ciphers on;
	ssl_ciphers 'kEECDH+ECDSA+AES128 kEECDH+ECDSA+AES256 kEECDH+AES128 kEECDH+AES256 kEDH+AES128 kEDH+AES256 DES-CBC3-SHA +SHA !aNULL !eNULL !LOW !MD5 !EXP !DSS !PSK !SRP !kECDH !CAMELLIA !RC4 !SEED';
	ssl_protocols TLSv1.2 TLSv1.1 TLSv1;
	ssl_session_cache   shared:SSL:10m;
	ssl_session_timeout 10m;
	keepalive_timeout   70;
	ssl_buffer_size 1400;

	gzip            on;
	gzip_min_length 1000;
	gzip_proxied    expired no-cache no-store private auth;
	gzip_types      text/plain text/css application/javascript application/xml application/json application/x-font-ttf font/opentype application/font-woff image/svg+xml application/vnd.ms-fontobject;

	location /v0 {
		proxy_pass                            http://127.0.0.1:8080/client/;
		proxy_set_header Host                 $host;
		proxy_set_header X-Real-IP            $remote_addr;
		proxy_set_header X-Forwarded-For      $proxy_add_x_forwarded_for;
		proxy_set_header X-Forwarded-Proto    $scheme;
	}

	location / {
		autoindex off;
		root /home/fules/src/esp8266_gps_unit/client/www/;
		
		# For SPA routers: if a virtual route is requested, serve the app instead
		try_files $uri $uri/ /index.html;
		
		auth_basic "Enter your credentials please";
		auth_basic_user_file /etc/nginx/htpasswd/beta.wodeewa.com.htpasswd;
	}

}
