
print("======= uvdrv started ======") 


local uvdriver = {
	c     		= require "uvdrv.c", 
	tcp   		= require "uvdrv.tcp", 
	util  		= require("uvdriver.util"), 
	timeout 	= require("uvdriver.timeout"),
	queue 		= require("uvdriver.queue"),
	socketdef   = require("uvdriver.socket"),
}

return uvdriver 

