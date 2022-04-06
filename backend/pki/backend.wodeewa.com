server {
	server_name backend.wodeewa.com;
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

	ssl_client_certificate /etc/nginx/pki/iot_ca.crt;
	ssl_verify_client on;
	ssl_verify_depth 2;

	location /v0 {
		if ($ssl_client_verify != SUCCESS) {
			return 403;
		}
		proxy_pass                            http://127.0.0.1:8080/backend/v0;
		proxy_set_header Host                 $host;
		proxy_set_header X-Real-IP            $remote_addr;
		proxy_set_header X-Forwarded-For      $proxy_add_x_forwarded_for;
		proxy_set_header X-Forwarded-Proto    $scheme;
		proxy_set_header X-SSL-Verify         $ssl_client_verify;
		proxy_set_header X-SSL-SID            $ssl_session_id;
		proxy_set_header X-SSL-Subject-DN     $ssl_client_s_dn;
		# http://nginx.org/en/docs/http/ngx_http_ssl_module.html#variables
	}

	location /ota {
		if ($ssl_client_verify != SUCCESS) {
			return 403;
		}
		autoindex off;
		root /home/fules/src/esp8266_gps_unit/backend/;
	}

}
