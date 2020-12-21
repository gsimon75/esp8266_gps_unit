server {
	#listen 80;
	server_name ota.wodeewa.com;
	ssl_certificate /etc/nginx/pki/STAR_wodeewa_com.chained.crt;
	ssl_certificate_key /etc/nginx/pki/wodeewa_com.key;
	root /var/www/html/ota.wodeewa.com;
	include /etc/nginx/vhost_defaults.conf;

	ssl_client_certificate /etc/nginx/pki/fake_ca.crt;
	ssl_verify_client optional;
	ssl_verify_depth 2;

	location /out {
		if ($ssl_client_verify != SUCCESS) {
			return 403;
		}
		# http://nginx.org/en/docs/http/ngx_http_ssl_module.html#variables
		autoindex on;
	}

}
