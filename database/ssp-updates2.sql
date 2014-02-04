
DROP TABLE IF EXISTS calling_program ;
DROP TABLE IF EXISTS calling_program_number ;

CREATE TABLE calling_program(
	id SERIAL 
	,name varchar(32) not null
	,description varchar(256)
	,INDEX(name)
	,PRIMARY KEY(id)
) ;


CREATE TABLE calling_program_number(
	id SERIAL 
	,called_number_in varchar(32) not null
	,called_number_out varchar(32) 
	,calling_program_id BIGINT UNSIGNED not null
	,PRIMARY KEY(id)
   ,CONSTRAINT FK_cp_1 FOREIGN KEY (calling_program_id)
              REFERENCES calling_program (id) ON DELETE CASCADE ON UPDATE CASCADE
) ;

INSERT INTO calling_program (id,name,description) values (1,'Office of Legislation','Office of Legislation') ;
INSERT INTO calling_program (id,name,description) values (2,'Navigators','Navigators') ;
INSERT INTO calling_program (id,name,description) values (3,'Medicaid','Medicaid') ;
INSERT INTO calling_program (id,name,description) values (4,'Appeals','Appeals') ;
INSERT INTO calling_program (id,name,description) values (5,'Agent/Broker','Agent/Broker') ;

INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('6197900120',1) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('6197900121',2) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('6197900123',3) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('6197900124',4) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('2016030300',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('2142611070',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('3122050700',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('7272160100',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('4156550881',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('4434530209',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('5594700209',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('7753840209',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('8629020209',5) ;
INSERT INTO calling_program_number (called_number_in,calling_program_id) values ('6197900122',5) ;
