
create database yourdb;
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    passwd char(50) NULL
)ENGINE=InnoDB;
INSERT INTO user(username, passwd) VALUES('zdl', '123456');