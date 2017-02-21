<?php

if(isset($_POST["text"]))
{
	$input = $_POST["text"];
	file_put_contents("data.txt", $input, FILE_APPEND);
}

if(isset($_POST["image"]))
{
	$input = $_POST["image"];
	$file = fopen(time()."_image.png", 'wb');
    fwrite($file, $input);
    fclose($file);
}

?>