# Production Deployment Guide

> Complete guide for deploying the professional video format support system in production environments.

## 📋 **Table of Contents**

1. [Pre-Deployment Checklist](#pre-deployment-checklist)
2. [System Requirements](#system-requirements)
3. [Installation Procedures](#installation-procedures)
4. [Configuration Management](#configuration-management)
5. [Security Considerations](#security-considerations)
6. [Performance Optimization](#performance-optimization)
7. [Monitoring & Maintenance](#monitoring--maintenance)
8. [Troubleshooting Guide](#troubleshooting-guide)

---

## ✅ **Pre-Deployment Checklist**

### **System Validation**
- [ ] **Hardware Requirements Met**
  - CPU: Multi-core processor (8+ cores recommended)
  - RAM: 16GB minimum, 32GB+ recommended
  - Storage: SSD with 100GB+ free space
  - GPU: Hardware acceleration support (optional but recommended)

- [ ] **Software Dependencies**
  - Operating System: Windows 10/11, Linux (Ubuntu 20.04+), macOS 10.15+
  - CMake 3.20+
  - Visual Studio 2019+ (Windows) or GCC 9+ (Linux) or Xcode 12+ (macOS)
  - Qt 6.2+ (for GUI applications)
  - FFmpeg 5.0+ libraries
  - vcpkg package manager

- [ ] **Network Requirements**
  - Internet access for initial setup and updates
  - Firewall configuration for streaming protocols (if applicable)
  - Bandwidth sufficient for target content resolution and bitrate

### **Quality Assurance**
- [ ] **Comprehensive Testing Complete**
  - All unit tests passing (100% pass rate required)
  - Integration tests validated
  - Performance benchmarks within acceptable ranges
  - Compatibility tests for target hardware completed

- [ ] **Format Support Validation**
  - All required video codecs tested and working
  - All required audio codecs tested and working
  - Container format support verified
  - Professional workflow validation complete

- [ ] **Security Assessment**
  - Security vulnerability scan completed
  - Access controls configured and tested
  - Data encryption verified (if applicable)
  - Audit logging functional

### **Documentation Review**
- [ ] **User Documentation**
  - User guides updated and reviewed
  - API documentation current
  - Installation instructions verified
  - Troubleshooting guides complete

- [ ] **Operational Documentation**
  - Monitoring procedures documented
  - Backup and recovery procedures tested
  - Maintenance schedules defined
  - Escalation procedures established

---

## 💻 **System Requirements**

### **Minimum Requirements**

#### **Hardware**
```yaml
CPU:
  Cores: 4
  Architecture: x64
  Speed: 2.5 GHz
  
Memory:
  RAM: 8 GB
  Available: 4 GB free
  
Storage:
  Type: HDD/SSD
  Free Space: 50 GB
  Speed: 100 MB/s read/write
  
Network:
  Bandwidth: 100 Mbps
  Latency: <100ms
```

#### **Software**
```yaml
Operating System:
  Windows: 10 version 1909+
  Linux: Ubuntu 18.04+ / CentOS 8+
  macOS: 10.14+
  
Runtime Dependencies:
  - Visual C++ Redistributable 2019+ (Windows)
  - libstdc++6 (Linux)
  - System frameworks (macOS)
  
Graphics:
  - DirectX 11+ support (Windows)
  - OpenGL 3.3+ support
  - Hardware decoding support (recommended)
```

### **Recommended Requirements**

#### **Hardware**
```yaml
CPU:
  Cores: 8+
  Architecture: x64
  Speed: 3.0+ GHz
  Features: AVX2, hardware video decode
  
Memory:
  RAM: 32 GB
  Available: 16 GB free
  Speed: DDR4-3200+
  
Storage:
  Type: NVMe SSD
  Free Space: 200 GB
  Speed: 1000+ MB/s read/write
  
Graphics:
  Dedicated GPU: NVIDIA RTX/GTX, AMD RX series
  VRAM: 4+ GB
  Features: Hardware encoding/decoding
  
Network:
  Bandwidth: 1 Gbps
  Latency: <10ms
```

### **High-Performance Requirements**

#### **Hardware**
```yaml
CPU:
  Cores: 16+
  Architecture: x64
  Speed: 3.5+ GHz
  Features: AVX2, AVX-512
  
Memory:
  RAM: 64+ GB
  Available: 32+ GB free
  Speed: DDR4-3200+
  ECC: Recommended
  
Storage:
  Type: NVMe SSD RAID
  Free Space: 1+ TB
  Speed: 3000+ MB/s read/write
  
Graphics:
  Multiple GPUs: Professional cards preferred
  VRAM: 8+ GB per GPU
  Features: Multi-stream encoding/decoding
  
Network:
  Bandwidth: 10+ Gbps
  Latency: <1ms
  Redundancy: Multiple connections
```

---

## 🚀 **Installation Procedures**

### **Windows Installation**

#### **Step 1: Prepare Environment**
```powershell
# Run as Administrator
# Install prerequisites
winget install Microsoft.VisualStudio.2022.Community
winget install Kitware.CMake
winget install Git.Git

# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg.exe integrate install
```

#### **Step 2: Install Video Editor**
```powershell
# Download release package
Invoke-WebRequest -Uri "https://releases.video-editor.com/v1.0.0/video-editor-windows.zip" -OutFile "video-editor.zip"
Expand-Archive -Path "video-editor.zip" -DestinationPath "C:\Program Files\VideoEditor"

# Install dependencies
C:\vcpkg\vcpkg.exe install ffmpeg[avresample,nonfree]:x64-windows
C:\vcpkg\vcpkg.exe install qt6[base,multimedia]:x64-windows

# Configure environment
[Environment]::SetEnvironmentVariable("VIDEO_EDITOR_HOME", "C:\Program Files\VideoEditor", "Machine")
[Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\Program Files\VideoEditor\bin", "Machine")
```

#### **Step 3: Configuration**
```powershell
# Copy configuration templates
Copy-Item "C:\Program Files\VideoEditor\config\templates\*" "C:\Program Files\VideoEditor\config\"

# Configure for production
$config = @"
{
  "environment": "production",
  "logging": {
    "level": "info",
    "output": "file",
    "path": "C:/ProgramData/VideoEditor/logs"
  },
  "performance": {
    "threads": "auto",
    "memory_limit_mb": 8192,
    "enable_hardware_acceleration": true
  },
  "security": {
    "enable_audit_logging": true,
    "enforce_format_validation": true
  }
}
"@
$config | Out-File -FilePath "C:\Program Files\VideoEditor\config\production.json" -Encoding UTF8
```

### **Linux Installation (Ubuntu/Debian)**

#### **Step 1: Prepare Environment**
```bash
#!/bin/bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install build dependencies
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    qt6-base-dev \
    qt6-multimedia-dev

# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
sudo /opt/vcpkg/bootstrap-vcpkg.sh
sudo /opt/vcpkg/vcpkg integrate install
```

#### **Step 2: Install Video Editor**
```bash
# Download and extract
wget https://releases.video-editor.com/v1.0.0/video-editor-linux.tar.gz
sudo tar -xzf video-editor-linux.tar.gz -C /opt/

# Create symlinks
sudo ln -sf /opt/video-editor/bin/video-editor /usr/local/bin/video-editor
sudo ln -sf /opt/video-editor/lib/* /usr/local/lib/

# Update library cache
sudo ldconfig
```

#### **Step 3: Service Configuration**
```bash
# Create systemd service
sudo tee /etc/systemd/system/video-editor.service > /dev/null <<EOF
[Unit]
Description=Video Editor Service
After=network.target

[Service]
Type=notify
User=video-editor
Group=video-editor
ExecStart=/opt/video-editor/bin/video-editor-service
ExecReload=/bin/kill -HUP \$MAINPID
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Create user and directories
sudo useradd -r -s /bin/false video-editor
sudo mkdir -p /var/log/video-editor /var/lib/video-editor
sudo chown video-editor:video-editor /var/log/video-editor /var/lib/video-editor

# Enable and start service
sudo systemctl enable video-editor
sudo systemctl start video-editor
```

### **macOS Installation**

#### **Step 1: Prepare Environment**
```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake git pkg-config ffmpeg qt6
```

#### **Step 2: Install Video Editor**
```bash
# Download and install
curl -L https://releases.video-editor.com/v1.0.0/video-editor-macos.dmg -o video-editor.dmg
hdiutil attach video-editor.dmg
sudo cp -R "/Volumes/Video Editor/Video Editor.app" /Applications/
hdiutil detach "/Volumes/Video Editor"

# Install command line tools
sudo mkdir -p /usr/local/bin
sudo ln -sf "/Applications/Video Editor.app/Contents/MacOS/video-editor" /usr/local/bin/video-editor
```

---

## ⚙️ **Configuration Management**

### **Environment-Specific Configuration**

#### **Production Configuration**
```json
{
  "environment": "production",
  "logging": {
    "level": "info",
    "output": "file",
    "path": "/var/log/video-editor",
    "max_file_size_mb": 100,
    "max_files": 10,
    "enable_structured_logging": true
  },
  "performance": {
    "threads": "auto",
    "memory_limit_mb": 16384,
    "cache_size_mb": 4096,
    "enable_hardware_acceleration": true,
    "optimization_level": "balanced"
  },
  "security": {
    "enable_audit_logging": true,
    "enforce_format_validation": true,
    "allow_experimental_codecs": false,
    "require_signed_plugins": true
  },
  "formats": {
    "enable_all_codecs": true,
    "preferred_decoder": "hardware",
    "preferred_encoder": "hardware",
    "quality_preset": "professional"
  },
  "monitoring": {
    "enable_health_checks": true,
    "health_check_interval_seconds": 60,
    "enable_performance_metrics": true,
    "metrics_collection_interval_seconds": 30
  }
}
```

#### **Development Configuration**
```json
{
  "environment": "development",
  "logging": {
    "level": "debug",
    "output": "console",
    "enable_debug_output": true
  },
  "performance": {
    "threads": 4,
    "memory_limit_mb": 8192,
    "enable_debugging": true
  },
  "security": {
    "enable_audit_logging": false,
    "allow_experimental_codecs": true,
    "require_signed_plugins": false
  },
  "development": {
    "enable_hot_reload": true,
    "enable_test_endpoints": true,
    "mock_hardware_acceleration": false
  }
}
```

### **Plugin Configuration**

#### **Codec Plugin Settings**
```json
{
  "plugins": {
    "h264_plugin": {
      "enabled": true,
      "hardware_acceleration": true,
      "quality_preset": "balanced",
      "max_decode_threads": 8,
      "max_encode_threads": 4
    },
    "hevc_plugin": {
      "enabled": true,
      "hardware_acceleration": true,
      "quality_preset": "high",
      "enable_10bit": true,
      "enable_hdr": true
    },
    "av1_plugin": {
      "enabled": true,
      "hardware_acceleration": false,
      "cpu_optimization": "speed",
      "tiles": "auto"
    }
  }
}
```

### **Quality Settings**
```json
{
  "quality": {
    "validation": {
      "enable_automatic_validation": true,
      "validation_level": "professional",
      "fail_on_quality_issues": true,
      "generate_quality_reports": true
    },
    "standards_compliance": {
      "broadcast_standards": ["ITU-R BT.709", "ITU-R BT.2020"],
      "streaming_standards": ["HLS", "DASH"],
      "enforce_compliance": true
    },
    "thresholds": {
      "min_psnr_db": 40.0,
      "min_ssim": 0.95,
      "max_encoding_time_ratio": 0.5
    }
  }
}
```

---

## 🔒 **Security Considerations**

### **Access Control**

#### **User Management**
```bash
# Create service account (Linux)
sudo useradd -r -s /bin/false -d /var/lib/video-editor video-editor
sudo usermod -aG video video-editor  # Hardware access
sudo usermod -aG audio video-editor  # Audio device access

# Set directory permissions
sudo mkdir -p /var/lib/video-editor/{config,cache,logs,temp}
sudo chown -R video-editor:video-editor /var/lib/video-editor
sudo chmod 750 /var/lib/video-editor
sudo chmod 640 /var/lib/video-editor/config/*
```

#### **File System Security**
```bash
# Secure configuration files
sudo chmod 600 /etc/video-editor/production.json
sudo chown root:video-editor /etc/video-editor/production.json

# Secure log files
sudo chmod 644 /var/log/video-editor/*.log
sudo chown video-editor:video-editor /var/log/video-editor/*.log

# Secure temporary directories
sudo mkdir -p /tmp/video-editor
sudo chmod 1777 /tmp/video-editor  # Sticky bit for temp directory
```

### **Network Security**

#### **Firewall Configuration**
```bash
# UFW configuration (Ubuntu)
sudo ufw allow from 192.168.1.0/24 to any port 8080  # API access
sudo ufw allow from 192.168.1.0/24 to any port 8443  # HTTPS API
sudo ufw deny 8080  # Block external access to HTTP API
sudo ufw enable
```

#### **TLS/SSL Configuration**
```json
{
  "network": {
    "api": {
      "bind_address": "127.0.0.1",
      "port": 8443,
      "tls": {
        "enabled": true,
        "certificate_file": "/etc/ssl/certs/video-editor.crt",
        "private_key_file": "/etc/ssl/private/video-editor.key",
        "min_tls_version": "1.2",
        "cipher_suites": [
          "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
          "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"
        ]
      }
    }
  }
}
```

### **Data Protection**

#### **Encryption at Rest**
```json
{
  "security": {
    "encryption": {
      "enable_cache_encryption": true,
      "encryption_algorithm": "AES-256-GCM",
      "key_rotation_days": 90,
      "secure_temp_files": true
    },
    "data_retention": {
      "temp_file_retention_hours": 24,
      "log_retention_days": 30,
      "cache_retention_days": 7
    }
  }
}
```

---

## ⚡ **Performance Optimization**

### **System-Level Optimization**

#### **CPU Optimization**
```bash
# Linux: CPU governor settings
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable CPU throttling for video processing
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
```

#### **Memory Optimization**
```bash
# Configure swappiness for video processing workloads
echo 'vm.swappiness=10' | sudo tee -a /etc/sysctl.conf

# Increase file handle limits
echo 'video-editor soft nofile 65536' | sudo tee -a /etc/security/limits.conf
echo 'video-editor hard nofile 65536' | sudo tee -a /etc/security/limits.conf
```

### **Application-Level Optimization**

#### **Threading Configuration**
```json
{
  "performance": {
    "threading": {
      "decode_threads": "auto",
      "encode_threads": "auto",
      "max_concurrent_operations": 4,
      "thread_affinity": true,
      "numa_awareness": true
    },
    "memory": {
      "frame_pool_size": 100,
      "buffer_pool_size_mb": 1024,
      "enable_memory_mapping": true,
      "prealloc_buffers": true
    },
    "io": {
      "io_threads": 2,
      "read_buffer_size_mb": 64,
      "write_buffer_size_mb": 64,
      "enable_direct_io": true
    }
  }
}
```

#### **Hardware Acceleration**
```json
{
  "hardware": {
    "video_acceleration": {
      "enable_nvidia_nvenc": true,
      "enable_intel_quicksync": true,
      "enable_amd_vce": true,
      "prefer_hardware_decode": true,
      "prefer_hardware_encode": true
    },
    "gpu_compute": {
      "enable_cuda": true,
      "enable_opencl": true,
      "max_gpu_memory_usage": 0.8
    }
  }
}
```

### **Monitoring Performance**

#### **Performance Metrics Collection**
```json
{
  "monitoring": {
    "metrics": {
      "enable_cpu_metrics": true,
      "enable_memory_metrics": true,
      "enable_io_metrics": true,
      "enable_gpu_metrics": true,
      "collection_interval_ms": 1000
    },
    "alerts": {
      "cpu_usage_threshold": 0.9,
      "memory_usage_threshold": 0.85,
      "gpu_usage_threshold": 0.95,
      "encoding_failure_threshold": 0.05
    }
  }
}
```

---

## 📊 **Monitoring & Maintenance**

### **Health Monitoring**

#### **System Health Checks**
```bash
#!/bin/bash
# health_check.sh

# Check service status
if ! systemctl is-active --quiet video-editor; then
    echo "ERROR: Video Editor service is not running"
    exit 1
fi

# Check disk space
DISK_USAGE=$(df /var/lib/video-editor | awk 'NR==2 {print $5}' | sed 's/%//')
if [ $DISK_USAGE -gt 90 ]; then
    echo "WARNING: Disk usage is $DISK_USAGE%"
fi

# Check memory usage
MEMORY_USAGE=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
if [ $MEMORY_USAGE -gt 90 ]; then
    echo "WARNING: Memory usage is $MEMORY_USAGE%"
fi

# Check log for errors
ERROR_COUNT=$(tail -n 1000 /var/log/video-editor/error.log | grep "ERROR" | wc -l)
if [ $ERROR_COUNT -gt 10 ]; then
    echo "WARNING: $ERROR_COUNT errors in last 1000 log lines"
fi

echo "Health check passed"
```

#### **Performance Monitoring**
```bash
#!/bin/bash
# performance_monitor.sh

# Collect system metrics
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
CPU_USAGE=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | sed 's/%us,//')
MEMORY_USAGE=$(free | awk 'NR==2{printf "%.1f", $3*100/$2}')
LOAD_AVERAGE=$(uptime | awk -F'load average:' '{print $2}' | awk '{print $1}' | sed 's/,//')

# Log metrics
echo "$TIMESTAMP,CPU:$CPU_USAGE,Memory:$MEMORY_USAGE,Load:$LOAD_AVERAGE" >> /var/log/video-editor/performance.log

# Check thresholds
if (( $(echo "$CPU_USAGE > 90" | bc -l) )); then
    echo "ALERT: High CPU usage: $CPU_USAGE%"
    # Send alert notification
fi
```

### **Automated Maintenance**

#### **Log Rotation**
```bash
# /etc/logrotate.d/video-editor
/var/log/video-editor/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    sharedscripts
    postrotate
        systemctl reload video-editor
    endscript
}
```

#### **Cache Cleanup**
```bash
#!/bin/bash
# cache_cleanup.sh

# Clean old cache files (older than 7 days)
find /var/lib/video-editor/cache -type f -mtime +7 -delete

# Clean temporary files (older than 1 day)
find /tmp/video-editor -type f -mtime +1 -delete

# Clean old log files
find /var/log/video-editor -name "*.log.*" -mtime +30 -delete

# Restart service if memory usage is high
MEMORY_USAGE=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
if [ $MEMORY_USAGE -gt 95 ]; then
    echo "High memory usage detected, restarting service"
    systemctl restart video-editor
fi
```

### **Backup Procedures**

#### **Configuration Backup**
```bash
#!/bin/bash
# backup_config.sh

BACKUP_DIR="/var/backups/video-editor"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
BACKUP_FILE="$BACKUP_DIR/config_backup_$TIMESTAMP.tar.gz"

mkdir -p $BACKUP_DIR

# Backup configuration files
tar -czf $BACKUP_FILE \
    /etc/video-editor/ \
    /var/lib/video-editor/config/ \
    /var/lib/video-editor/plugins/

# Keep only last 10 backups
ls -t $BACKUP_DIR/config_backup_*.tar.gz | tail -n +11 | xargs rm -f

echo "Configuration backed up to $BACKUP_FILE"
```

---

## 🔧 **Troubleshooting Guide**

### **Common Issues**

#### **Installation Issues**

**Issue: Missing Dependencies**
```bash
# Symptoms
ERROR: Could not find required library: libavcodec.so.58

# Solution (Ubuntu/Debian)
sudo apt update
sudo apt install ffmpeg libavcodec-dev libavformat-dev

# Solution (CentOS/RHEL)
sudo yum install epel-release
sudo yum install ffmpeg-devel
```

**Issue: Permission Denied**
```bash
# Symptoms
Permission denied: /var/lib/video-editor/cache

# Solution
sudo chown -R video-editor:video-editor /var/lib/video-editor
sudo chmod -R 755 /var/lib/video-editor
```

#### **Runtime Issues**

**Issue: Hardware Acceleration Not Working**
```json
// Check hardware support
{
  "hardware_check": {
    "nvidia_support": false,
    "intel_quicksync": false,
    "error": "No compatible hardware found"
  }
}

// Solution: Update drivers
// NVIDIA: Download latest drivers from nvidia.com
// Intel: Update Intel Graphics drivers
// AMD: Update AMD drivers
```

**Issue: High Memory Usage**
```bash
# Symptoms
Out of memory errors, system slowdown

# Diagnosis
ps aux | grep video-editor | awk '{print $4}' | head -1  # Check memory usage

# Solution
# 1. Increase memory limits in configuration
# 2. Reduce concurrent operations
# 3. Clear cache
rm -rf /var/lib/video-editor/cache/*
systemctl restart video-editor
```

#### **Performance Issues**

**Issue: Slow Encoding Performance**
```bash
# Diagnosis steps
1. Check CPU usage: top -p $(pgrep video-editor)
2. Check GPU usage: nvidia-smi (if NVIDIA GPU)
3. Check I/O wait: iostat -x 1
4. Check memory usage: free -h

# Solutions
1. Enable hardware acceleration
2. Increase thread count
3. Use faster storage (SSD)
4. Optimize encoding settings
```

### **Debug Information Collection**

#### **System Information Script**
```bash
#!/bin/bash
# collect_debug_info.sh

echo "=== Video Editor Debug Information ==="
echo "Timestamp: $(date)"
echo

echo "=== System Information ==="
uname -a
cat /etc/os-release
echo

echo "=== Hardware Information ==="
lscpu | head -20
free -h
lspci | grep -i vga
echo

echo "=== Service Status ==="
systemctl status video-editor
echo

echo "=== Recent Logs ==="
tail -n 50 /var/log/video-editor/error.log
echo

echo "=== Configuration ==="
cat /etc/video-editor/production.json
echo

echo "=== Process Information ==="
ps aux | grep video-editor
echo

echo "=== Network Status ==="
netstat -tlnp | grep video-editor
echo

echo "=== Disk Usage ==="
df -h /var/lib/video-editor
echo

echo "=== Debug Information Collection Complete ==="
```

### **Performance Diagnostics**

#### **Performance Profiling**
```bash
#!/bin/bash
# performance_profile.sh

# Profile for 60 seconds
echo "Starting performance profiling..."

# CPU profiling
perf record -p $(pgrep video-editor) sleep 60
perf report > /tmp/cpu_profile.txt

# Memory profiling
valgrind --tool=massif --pid=$(pgrep video-editor) &
VALGRIND_PID=$!
sleep 60
kill $VALGRIND_PID

# I/O profiling
iotop -a -o -d 1 -t -p $(pgrep video-editor) > /tmp/io_profile.txt &
IOTOP_PID=$!
sleep 60
kill $IOTOP_PID

echo "Profiling complete. Results in /tmp/"
```

### **Recovery Procedures**

#### **Service Recovery**
```bash
#!/bin/bash
# service_recovery.sh

echo "Starting service recovery..."

# Stop service
systemctl stop video-editor

# Clear temporary files
rm -rf /tmp/video-editor/*
rm -rf /var/lib/video-editor/temp/*

# Clear cache if corrupted
if [ "$1" == "clear-cache" ]; then
    rm -rf /var/lib/video-editor/cache/*
fi

# Restore configuration from backup
if [ -f /var/backups/video-editor/config_backup_latest.tar.gz ]; then
    tar -xzf /var/backups/video-editor/config_backup_latest.tar.gz -C /
fi

# Start service
systemctl start video-editor

# Verify service health
sleep 5
if systemctl is-active --quiet video-editor; then
    echo "Service recovery successful"
else
    echo "Service recovery failed"
    systemctl status video-editor
fi
```

---

This production deployment guide provides comprehensive instructions for deploying, configuring, and maintaining the professional video format support system in production environments. Follow these procedures carefully to ensure a reliable and secure deployment.
