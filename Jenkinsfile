pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                // Get some code from a GitHub repository
                git 'https://github.com/AntonellaDc/wazuh.git'

                // Run install.
		sh "sudo su"
                sh "./install.sh" 
            }

            
        }
    }
}
