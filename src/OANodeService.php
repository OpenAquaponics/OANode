<?php

//curl -i -X POST -H "Content-Type: application/json" -d "{\"sData\":\"32,543,456.4\"}" http://localhost/index.php/v1/nestinator/OANodes/624B0F36/data

// Setup the path to all of the directories
defined('SERVICEPATH')||define('SERVICEPATH', '/var/www/html/service/');
defined('CFGPATH')||define('CFGPATH', SERVICEPATH);
defined('CFGFILE')||define('CFGFILE', 'config.json');


// The OANode.service class (handles sampling, logging, and transmitting data
class OANodeService {
  protected $cpid = -1;
  protected $file_cnt = 0;

  public function sendJSON($path, $data_string) {
    $ch = curl_init($path);
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'POST');
    curl_setopt($ch, CURLOPT_POSTFIELDS, $data_string);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array(
      'Content-Type: application/json',
      'Content-Length: ' . strlen($data_string))
    );
    return(curl_exec($ch));
  }

  public function run() {
    $this->cfg  = json_decode(file_get_contents(CFGPATH.CFGFILE));
    $outfile_name = '';
 
    // Make sure the required fields are included
    if(!isset($this->cfg->sUsername)) {
      print 'sUsername required'; exit(1);
    }
    if(!isset($this->cfg->sNodeId)) {
      print 'sNodeId required'; exit(1);
    }
    if(!isset($this->cfg->mPollingPeriod)) {
      print 'mPollingPeriod required'; exit(1);
    }
 
    // Loop the service indefinately
    while(1) {
      $start_time = microtime(true);

      // TODO - Do some kind of authentication
      // TODO - Should the config.json file be loaded everytime and update the system configure
 
      // Take a sample
      $data = array((string)time(), (string)time() + 132, (string)time() + 500);
      $data_string = json_encode(array("sData" => implode($data, ',')));
 
      // Build and send the data packet over the network
      $ret = $this->sendJSON('http://localhost/index.php/v1/'.$this->cfg->sUsername.'/OANodes/'.$this->cfg->sNodeId.'/data', $data_string);
 
      // Network failed or unavailable, save to disk for transfer later
      if(empty($ret)) {
        $this->outfile_name = SERVICEPATH.'data/LOG_'.str_pad((int)$this->file_cnt,5,STR_PAD_LEFT).'.dat';
        file_put_contents($this->outfile_name, $data_string."\n", FILE_APPEND);
        if(filesize($this->outfile_name) > 50000) {
          $this->file_cnt++;
        }
      }
      else {
        // Check to see if there is data that needs to be written to the server
        $this->sendPendingTransfers();
      }
 
      // Compensate for the execution time when computing the timeout period
      usleep(($this->cfg->mPollingPeriod - (microtime(true) - $start_time)) * 1000000);
    }
  }

  public function sendPendingTransfers() {
    // There is a child process running, monitor its status
    if($this->cpid > 0) {
      if($this->cpid == pcntl_waitpid(-1, $stat, WNOHANG)) {
        $this->cpid = -1;
      }
      // TODO - Add a high level monitor to automatically kill process after 10 min
      //   This could stop zombie tasks when I have an unexpected/unhandled error
      //   occur in the child task.
    }

    // There is no child process running, you can safely spawn a new one
    if($this->cpid <= 0) {
      $files = scandir(SERVICEPATH.'data', 1);
      // Skip files: '.' and '..'
      if(count($files) > 2) {
        if(($this->cpid = pcntl_fork()) == 0) {
          $this->uploadData($files);
        }
        $this->file_cnt++;
      }
      else {
        $this->file_cnt = 0;
      }
    }
  }

  public function uploadData($files) {
    // Skip files: '.' and '..'
    for($i = 0; $i < (count($files) - 2); $i++) {
      // TODO - There needs to be a better verification than seeing data was returned.
      // TODO - The server should return the number of objects in the "batch" or something
      // TODO - There needs to be better error handling

      $filename = SERVICEPATH.'data/'.$files[$i];

      // Build the JSON string
      $data_string = '{"batch":['.substr_replace(str_replace("\n", ",", file_get_contents($filename)), "", -1).']}';

      $ret = $this->sendJSON('http://localhost/index.php/v1/'.$this->cfg->sUsername.'/OANodes/'.$this->cfg->sNodeId.'/data', $data_string);
      if(!empty($ret)) {
        unlink($filename);
      }
    }
    exit(0);
  }

}

$app = new OANodeService();
$app->run();


