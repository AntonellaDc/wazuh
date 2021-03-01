pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
               
		echo 'Clone code from Github Repository'
                git  'https://github.com/AntonellaDc/wazuh.git'

                echo 'Compile the last released version'
		sh "chmod 777 install.sh"
                sh "./install.sh" 
            }

            
        }
    }
}
