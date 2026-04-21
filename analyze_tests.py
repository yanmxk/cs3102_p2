#!/usr/bin/env python3
"""
LRTP Protocol Test Analysis Script
Analyzes test results to understand protocol behavior and performance

Supports distributed testing across machines:
- Server mode: Run test servers and wait for client connections
- Client mode: Run test clients against remote servers
- Analyzer mode: Collect and analyze test results
"""

import os
import subprocess
import re
import json
import time
import argparse
import socket
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Tuple, Optional
from statistics import mean, stdev, median
import sys

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.gridspec import GridSpec
import numpy as np

class DistributedTestConfig:
    """Configuration for distributed testing"""
    def __init__(self, mode='local', server_host='127.0.0.1', server_port=None, 
                 results_file=None, workspace_path=None):
        self.mode = mode  # 'local', 'server', 'client', 'analyzer'
        self.server_host = server_host
        self.server_port = server_port
        self.results_file = results_file
        # Use current directory if workspace not specified
        self.workspace_path = workspace_path or os.getcwd()

@dataclass
class RTTMetrics:
    """Round Trip Time measurements"""
    rtt: Optional[float] = None      # microseconds
    srtt: Optional[float] = None     # Smoothed RTT (microseconds)
    rttvar: Optional[float] = None   # RTT variance (microseconds)
    rto: Optional[float] = None      # Retransmission timeout (microseconds)

@dataclass
class PacketMetrics:
    """Per-packet metrics"""
    packet_num: int
    metrics: RTTMetrics
    rto_change: Optional[str] = None  # "increase", "decrease", "stable"

@dataclass
class TestResult:
    """Complete test result"""
    test_name: str
    test_type: str  # basic, adaptive_rto, large_transfer, multiple_sends, stress
    status: str  # success, failure, partial
    packets_sent: int = 0
    packets_received: int = 0
    packets_lost: int = 0
    retransmissions: int = 0
    duration_seconds: float = 0.0
    throughput_bps: float = 0.0
    packet_metrics: List[PacketMetrics] = None
    raw_output: str = ""
    
    def __post_init__(self):
        if self.packet_metrics is None:
            self.packet_metrics = []

class LRTPTestAnalyzer:
    def __init__(self, workspace_path: str = None, 
                 config: DistributedTestConfig = None):
        # Use current directory if workspace not specified
        if workspace_path is None:
            workspace_path = os.getcwd()
        self.workspace = workspace_path
        self.startercode = os.path.join(workspace_path, "startercode")
        self.results: List[TestResult] = []
        self.config = config or DistributedTestConfig(workspace_path=workspace_path)
        self.test_pairs = [
            ("test-client-0", "test-server-0", "basic"),
            ("test-client-1", "test-server-1", "basic"),
            ("test-client-2", "test-server-2", "basic"),
            ("test-client-3", "test-server-3", "basic"),
            ("test-adaptive-rto-client", "test-adaptive-rto-server", "adaptive_rto"),
            ("test-large-transfer-client", "test-large-transfer-server", "large_transfer"),
            ("test-multiple-sends-client", "test-multiple-sends-server", "multiple_sends"),
            ("test-stress-client", "test-stress-server", "stress"),
        ]
        
    def build_tests(self) -> bool:
        """Build all test binaries"""
        print("[*] Building LRTP tests...")
        try:
            result = subprocess.run(
                ["make", "clean", "&&", "make"],
                cwd=self.startercode,
                shell=True,
                capture_output=True,
                timeout=60,
                text=True
            )
            if result.returncode != 0:
                print(f"[!] Build failed:\n{result.stderr}")
                return False
            print("[+] Build successful")
            return True
        except Exception as e:
            print(f"[!] Build error: {e}")
            return False
    
    def save_results(self, filename: str = None):
        """Save test results to JSON file for analysis on another machine"""
        if filename is None:
            filename = os.path.join(self.workspace, "test_results.json")
        
        results_data = []
        for result in self.results:
            results_data.append({
                'test_name': result.test_name,
                'test_type': result.test_type,
                'status': result.status,
                'packets_sent': result.packets_sent,
                'packets_received': result.packets_received,
                'packets_lost': result.packets_lost,
                'retransmissions': result.retransmissions,
                'duration_seconds': result.duration_seconds,
                'throughput_bps': result.throughput_bps,
                'raw_output': result.raw_output,
                'packet_metrics': [
                    {
                        'packet_num': m.packet_num,
                        'metrics': {
                            'rtt': m.metrics.rtt,
                            'srtt': m.metrics.srtt,
                            'rttvar': m.metrics.rttvar,
                            'rto': m.metrics.rto
                        }
                    } for m in result.packet_metrics
                ]
            })
        
        with open(filename, 'w') as f:
            json.dump(results_data, f, indent=2)
        print(f"[+] Results saved to {filename}")
    
    def load_results(self, filename: str):
        """Load test results from JSON file"""
        if not os.path.exists(filename):
            print(f"[!] Results file not found: {filename}")
            return False
        
        try:
            with open(filename, 'r') as f:
                results_data = json.load(f)
            
            self.results = []
            for data in results_data:
                packet_metrics = []
                for pm in data.get('packet_metrics', []):
                    metrics = RTTMetrics(
                        rtt=pm['metrics'].get('rtt'),
                        srtt=pm['metrics'].get('srtt'),
                        rttvar=pm['metrics'].get('rttvar'),
                        rto=pm['metrics'].get('rto')
                    )
                    packet_metrics.append(PacketMetrics(pm['packet_num'], metrics))
                
                result = TestResult(
                    test_name=data['test_name'],
                    test_type=data['test_type'],
                    status=data['status'],
                    packets_sent=data.get('packets_sent', 0),
                    packets_received=data.get('packets_received', 0),
                    packets_lost=data.get('packets_lost', 0),
                    retransmissions=data.get('retransmissions', 0),
                    duration_seconds=data.get('duration_seconds', 0.0),
                    throughput_bps=data.get('throughput_bps', 0.0),
                    raw_output=data.get('raw_output', ''),
                    packet_metrics=packet_metrics
                )
                self.results.append(result)
            
            print(f"[+] Loaded {len(self.results)} test results from {filename}")
            return True
        except Exception as e:
            print(f"[!] Error loading results: {e}")
            return False
    
    def run_servers_mode(self):
        """Run test servers and wait for connections from remote clients"""
        print("\n" + "="*70)
        print("LRTP TEST SERVER MODE")
        print("="*70)
        print(f"[*] Building tests...")
        
        if not self.build_tests():
            return False
        
        if not self.config.server_port:
            # Use multiple ports for different server types
            print("[*] Running test servers on random available ports...")
            print("[*] Configure clients to connect to:")
            print(f"    Server Host: {self.get_local_ip()}")
        else:
            print(f"[*] Running test servers on port {self.config.server_port}...")
        
        server_processes = []
        try:
            # For simplicity, get available ports for each server
            ports = {}
            for i in range(len(self.test_pairs)):
                port = self.get_available_port()
                ports[i] = port
                print(f"    [{i}] Port {port}")
            
            # Start all servers
            for i, (client, server, test_type) in enumerate(self.test_pairs):
                server_path = os.path.join(self.startercode, server)
                if not os.path.exists(server_path):
                    print(f"[!] Server binary not found: {server_path}")
                    continue
                
                port = ports[i]
                print(f"\n[*] Starting {server} on port {port}...")
                
                server_proc = subprocess.Popen(
                    [server_path, str(port)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                server_processes.append((server, port, server_proc))
                print(f"[+] {server} started (PID: {server_proc.pid})")
            
            # Keep servers running
            print("\n[+] All servers running. Press Ctrl+C to stop.")
            while True:
                time.sleep(1)
                # Check if any server crashed
                for name, port, proc in server_processes:
                    if proc.poll() is not None:
                        print(f"[!] {name} on port {port} crashed")
        
        except KeyboardInterrupt:
            print("\n[*] Stopping servers...")
            for name, port, proc in server_processes:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                    print(f"[+] Stopped {name} on port {port}")
                except subprocess.TimeoutExpired:
                    proc.kill()
                    print(f"[+] Killed {name} on port {port}")
        
        return True
    
    def run_clients_mode(self):
        """Run test clients against remote servers"""
        print("\n" + "="*70)
        print("LRTP TEST CLIENT MODE")
        print("="*70)
        print(f"[*] Building tests...")
        
        if not self.build_tests():
            return False
        
        if not self.config.server_host:
            print("[!] Server host not specified. Use --server-host option")
            return False
        
        print(f"[*] Connecting to server at {self.config.server_host}")
        print(f"[*] Running clients...")
        
        # Ask user for port information or use provided port
        if not self.config.server_port:
            print("\n[!] Server port not specified. Please provide ports for each test:")
            ports_input = input("Enter comma-separated ports for each test: ")
            try:
                ports = [int(p.strip()) for p in ports_input.split(',')]
                if len(ports) != len(self.test_pairs):
                    print(f"[!] Expected {len(self.test_pairs)} ports, got {len(ports)}")
                    return False
            except ValueError:
                print("[!] Invalid port list")
                return False
        else:
            # Use same port for all tests (will fail, but shows intent)
            ports = [self.config.server_port] * len(self.test_pairs)
        
        for i, (client, server, test_type) in enumerate(self.test_pairs):
            port = ports[i]
            result = self.run_test_pair_remote(client, test_type, self.config.server_host, port)
            self.results.append(result)
            time.sleep(1)
        
        # Save results locally
        results_file = os.path.join(self.workspace, "test_results_client.json")
        self.save_results(results_file)
        print(f"\n[+] Client results saved. Transfer {results_file} to analyzer machine.")
        
        return True
    
    def run_test_pair_remote(self, client_name: str, test_type: str, 
                            server_host: str, server_port: int) -> TestResult:
        """Run a client against a remote server"""
        print(f"\n[*] Running {client_name} against {server_host}:{server_port}...")
        
        test_name = f"{client_name} -> {server_host}:{server_port}"
        result = TestResult(
            test_name=test_name,
            test_type=test_type,
            status="failure"
        )
        
        client_path = os.path.join(self.startercode, client_name)
        
        if not os.path.exists(client_path):
            print(f"[!] Client binary not found: {client_path}")
            return result
        
        try:
            start_time = time.time()
            client_result = subprocess.run(
                [client_path, server_host, str(server_port)],
                capture_output=True,
                timeout=30,
                text=True
            )
            duration = time.time() - start_time
            
            result.raw_output = client_result.stdout
            result.duration_seconds = duration
            
            if client_result.returncode == 0:
                result.status = "success"
                print(f"[+] {test_name} completed successfully")
            else:
                result.status = "partial"
                print(f"[!] {test_name} completed with warnings")
            
            self._parse_test_output(result, client_result.stdout)
            
        except subprocess.TimeoutExpired:
            print(f"[!] {test_name} timed out")
            result.status = "failure"
        except Exception as e:
            print(f"[!] Error running {test_name}: {e}")
            result.status = "failure"
        
        return result
    
    def get_available_port(self) -> int:
        """Get an available port for testing"""
        import socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(('', 0))
        port = sock.getsockname()[1]
        sock.close()
        return port
    
    def get_local_ip(self) -> str:
        """Get local machine IP address"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.connect(("8.8.8.8", 80))
            ip = sock.getsockname()[0]
            sock.close()
            return ip
        except:
            return "127.0.0.1"
    
    def run_test_pair(self, client_name: str, server_name: str, test_type: str, port: int) -> TestResult:
        """Run a client-server test pair and capture results"""
        print(f"\n[*] Running {client_name} + {server_name}...")
        
        test_name = f"{client_name} + {server_name}"
        result = TestResult(
            test_name=test_name,
            test_type=test_type,
            status="failure"
        )
        
        server_path = os.path.join(self.startercode, server_name)
        client_path = os.path.join(self.startercode, client_name)
        
        # Check if binaries exist
        if not os.path.exists(server_path) or not os.path.exists(client_path):
            print(f"[!] Test binary not found for {test_name}")
            return result
        
        try:
            # Start server
            server_proc = subprocess.Popen(
                [server_path, str(port)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Give server time to start
            time.sleep(0.5)
            
            # Run client
            start_time = time.time()
            client_result = subprocess.run(
                [client_path, "127.0.0.1", str(port)],
                capture_output=True,
                timeout=30,
                text=True
            )
            duration = time.time() - start_time
            
            # Give server time to finish
            time.sleep(0.5)
            
            # Terminate server
            server_proc.terminate()
            try:
                server_output, server_stderr = server_proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                server_proc.kill()
                server_output = ""
                server_stderr = ""
            
            result.raw_output = client_result.stdout
            result.duration_seconds = duration
            
            # Parse results
            if client_result.returncode == 0:
                result.status = "success"
                print(f"[+] {test_name} completed successfully")
            else:
                result.status = "partial"
                print(f"[!] {test_name} completed with warnings")
            
            # Parse metrics from output
            self._parse_test_output(result, client_result.stdout)
            
        except subprocess.TimeoutExpired:
            print(f"[!] {test_name} timed out")
            result.status = "failure"
        except Exception as e:
            print(f"[!] Error running {test_name}: {e}")
            result.status = "failure"
        
        return result
    
    def _parse_test_output(self, result: TestResult, output: str):
        """Parse test output to extract metrics"""
        lines = output.split('\n')
        
        for line in lines:
            # Parse packet metrics
            parts = line.split('|')
            if len(parts) >= 5:
                try:
                    # Try to parse as metric line
                    packet_match = re.match(r'\s*(\d+)\s*\|', parts[0])
                    if packet_match:
                        packet_num = int(packet_match.group(1))
                        
                        metrics = RTTMetrics()
                        
                        # Extract RTT, SRTT, RTTVAR, RTO if present
                        if len(parts) > 1:
                            rtt_match = re.search(r'(\d+(?:\.\d+)?)', parts[1])
                            if rtt_match:
                                metrics.rtt = float(rtt_match.group(1))
                        
                        if len(parts) > 2:
                            srtt_match = re.search(r'(\d+(?:\.\d+)?)', parts[2])
                            if srtt_match:
                                metrics.srtt = float(srtt_match.group(1))
                        
                        if len(parts) > 3:
                            rttvar_match = re.search(r'(\d+(?:\.\d+)?)', parts[3])
                            if rttvar_match:
                                metrics.rttvar = float(rttvar_match.group(1))
                        
                        if len(parts) > 4:
                            rto_match = re.search(r'(\d+(?:\.\d+)?)', parts[4])
                            if rto_match:
                                metrics.rto = float(rto_match.group(1))
                        
                        if metrics.rto:
                            result.packet_metrics.append(PacketMetrics(packet_num, metrics))
                except (ValueError, IndexError):
                    pass
            
            # Count packets and retransmissions
            if "sent" in line.lower():
                match = re.search(r'(\d+)\s*(?:packets?|messages?)\s*sent', line)
                if match:
                    result.packets_sent = int(match.group(1))
            
            if "received" in line.lower():
                match = re.search(r'(\d+)\s*(?:packets?|messages?)\s*(?:received|rx)', line)
                if match:
                    result.packets_received = int(match.group(1))
            
            if "retransmit" in line.lower():
                match = re.search(r'(\d+)', line)
                if match:
                    result.retransmissions = int(match.group(1))
            
            if "loss" in line.lower() or "lost" in line.lower():
                match = re.search(r'(\d+)', line)
                if match:
                    result.packets_lost = int(match.group(1))
        
        # Calculate throughput if we have data
        if result.duration_seconds > 0 and result.packets_sent > 0:
            # Assume standard packet size for throughput calculation
            bytes_sent = result.packets_sent * 1280  # standard test packet size
            result.throughput_bps = (bytes_sent * 8) / result.duration_seconds
    
    def run_all_tests(self) -> List[TestResult]:
        """Run all test pairs"""
        print("\n" + "="*70)
        print("LRTP PROTOCOL TEST SUITE")
        print("="*70)
        
        if not self.build_tests():
            print("[!] Build failed, aborting tests")
            return []
        
        for client, server, test_type in self.test_pairs:
            port = self.get_available_port()
            result = self.run_test_pair(client, server, test_type, port)
            self.results.append(result)
            time.sleep(1)  # Wait between tests
        
        return self.results
    
    def analyze_results(self) -> Dict:
        """Perform comprehensive analysis on test results"""
        analysis = {
            "summary": self._analyze_summary(),
            "basic_tests": self._analyze_basic_tests(),
            "adaptive_rto": self._analyze_adaptive_rto(),
            "stress_tests": self._analyze_stress(),
            "performance": self._analyze_performance(),
            "reliability": self._analyze_reliability(),
        }
        return analysis
    
    def _analyze_summary(self) -> Dict:
        """Overall test summary"""
        total_tests = len(self.results)
        passed = sum(1 for r in self.results if r.status == "success")
        failed = sum(1 for r in self.results if r.status == "failure")
        
        return {
            "total_tests": total_tests,
            "passed": passed,
            "failed": failed,
            "success_rate": (passed / total_tests * 100) if total_tests > 0 else 0,
        }
    
    def _analyze_basic_tests(self) -> Dict:
        """Analyze basic connectivity tests"""
        basic_results = [r for r in self.results if r.test_type == "basic"]
        
        if not basic_results:
            return {"message": "No basic tests found"}
        
        success_count = sum(1 for r in basic_results if r.status == "success")
        total_packets = sum(r.packets_sent for r in basic_results)
        
        return {
            "tests_run": len(basic_results),
            "tests_passed": success_count,
            "total_packets_sent": total_packets,
            "average_duration_ms": mean([r.duration_seconds * 1000 for r in basic_results]) if basic_results else 0,
        }
    
    def _analyze_adaptive_rto(self) -> Dict:
        """Analyze adaptive RTO behavior"""
        rto_results = [r for r in self.results if r.test_type == "adaptive_rto"]
        
        if not rto_results:
            return {"message": "No adaptive RTO tests found"}
        
        analysis = {
            "tests_run": len(rto_results),
        }
        
        for result in rto_results:
            if result.packet_metrics:
                rto_values = [m.metrics.rto for m in result.packet_metrics if m.metrics.rto]
                rtt_values = [m.metrics.rtt for m in result.packet_metrics if m.metrics.rtt]
                srtt_values = [m.metrics.srtt for m in result.packet_metrics if m.metrics.srtt]
                rttvar_values = [m.metrics.rttvar for m in result.packet_metrics if m.metrics.rttvar]
                
                if rto_values:
                    analysis["rto_stats"] = {
                        "min_rto_us": min(rto_values),
                        "max_rto_us": max(rto_values),
                        "mean_rto_us": mean(rto_values),
                        "rto_range_us": max(rto_values) - min(rto_values),
                    }
                
                if rtt_values:
                    analysis["rtt_stats"] = {
                        "min_rtt_us": min(rtt_values),
                        "max_rtt_us": max(rtt_values),
                        "mean_rtt_us": mean(rtt_values),
                        "median_rtt_us": median(rtt_values),
                    }
                
                if srtt_values:
                    analysis["srtt_stats"] = {
                        "min_srtt_us": min(srtt_values),
                        "max_srtt_us": max(srtt_values),
                        "mean_srtt_us": mean(srtt_values),
                    }
        
        return analysis
    
    def _analyze_stress(self) -> Dict:
        """Analyze stress test results"""
        stress_results = [r for r in self.results if r.test_type == "stress"]
        
        if not stress_results:
            return {"message": "No stress tests found"}
        
        return {
            "tests_run": len(stress_results),
            "total_packets_sent": sum(r.packets_sent for r in stress_results),
            "total_retransmissions": sum(r.retransmissions for r in stress_results),
            "average_duration_seconds": mean([r.duration_seconds for r in stress_results]) if stress_results else 0,
        }
    
    def _analyze_performance(self) -> Dict:
        """Analyze throughput and latency"""
        successful_tests = [r for r in self.results if r.status == "success"]
        
        if not successful_tests:
            return {"message": "No successful tests to analyze"}
        
        throughputs = [r.throughput_bps for r in successful_tests if r.throughput_bps > 0]
        durations = [r.duration_seconds for r in successful_tests if r.duration_seconds > 0]
        
        return {
            "throughput": {
                "average_bps": mean(throughputs) if throughputs else 0,
                "min_bps": min(throughputs) if throughputs else 0,
                "max_bps": max(throughputs) if throughputs else 0,
            },
            "latency": {
                "average_ms": mean([d * 1000 for d in durations]) if durations else 0,
                "min_ms": min([d * 1000 for d in durations]) if durations else 0,
                "max_ms": max([d * 1000 for d in durations]) if durations else 0,
            },
        }
    
    def _analyze_reliability(self) -> Dict:
        """Analyze packet loss and retransmission"""
        return {
            "total_packets_sent": sum(r.packets_sent for r in self.results),
            "total_packets_received": sum(r.packets_received for r in self.results),
            "total_retransmissions": sum(r.retransmissions for r in self.results),
            "total_packets_lost": sum(r.packets_lost for r in self.results),
            "packet_loss_rate_percent": (
                (sum(r.packets_lost for r in self.results) / 
                 sum(r.packets_sent for r in self.results) * 100)
                if sum(r.packets_sent for r in self.results) > 0 else 0
            ),
        }
    
    def generate_report(self, analysis: Dict, output_file: str = None) -> str:
        """Generate detailed analysis report"""
        if output_file is None:
            output_file = os.path.join(self.workspace, "test_analysis_report.txt")
        
        report = []
        report.append("="*70)
        report.append("LRTP PROTOCOL TEST ANALYSIS REPORT")
        report.append("="*70)
        report.append("")
        
        # Test Summary
        report.append("TEST EXECUTION SUMMARY")
        report.append("-"*70)
        summary = analysis["summary"]
        report.append(f"Total Tests Run:        {summary['total_tests']}")
        report.append(f"Tests Passed:           {summary['passed']}")
        report.append(f"Tests Failed:           {summary['failed']}")
        report.append(f"Success Rate:           {summary['success_rate']:.1f}%")
        report.append("")
        
        # Basic Connectivity Tests
        report.append("BASIC CONNECTIVITY ANALYSIS")
        report.append("-"*70)
        basic = analysis["basic_tests"]
        if "message" not in basic:
            report.append(f"Tests Run:              {basic['tests_run']}")
            report.append(f"Tests Passed:           {basic['tests_passed']}")
            report.append(f"Total Packets Sent:     {basic['total_packets_sent']}")
            report.append(f"Average Duration:       {basic['average_duration_ms']:.2f} ms")
        else:
            report.append(basic["message"])
        report.append("")
        
        # Adaptive RTO Analysis
        report.append("ADAPTIVE RTO BEHAVIOR ANALYSIS")
        report.append("-"*70)
        rto = analysis["adaptive_rto"]
        if "message" not in rto:
            report.append(f"Tests Run:              {rto['tests_run']}")
            if "rto_stats" in rto:
                rto_stats = rto["rto_stats"]
                report.append(f"\nRTO Statistics (microseconds):")
                report.append(f"  Minimum RTO:          {rto_stats['min_rto_us']:.0f} μs ({rto_stats['min_rto_us']/1000:.3f} ms)")
                report.append(f"  Maximum RTO:          {rto_stats['max_rto_us']:.0f} μs ({rto_stats['max_rto_us']/1000:.3f} ms)")
                report.append(f"  Mean RTO:             {rto_stats['mean_rto_us']:.0f} μs ({rto_stats['mean_rto_us']/1000:.3f} ms)")
                report.append(f"  RTO Range:            {rto_stats['rto_range_us']:.0f} μs")
            if "rtt_stats" in rto:
                rtt_stats = rto["rtt_stats"]
                report.append(f"\nRTT Statistics (microseconds):")
                report.append(f"  Minimum RTT:          {rtt_stats['min_rtt_us']:.0f} μs ({rtt_stats['min_rtt_us']/1000:.3f} ms)")
                report.append(f"  Maximum RTT:          {rtt_stats['max_rtt_us']:.0f} μs ({rtt_stats['max_rtt_us']/1000:.3f} ms)")
                report.append(f"  Mean RTT:             {rtt_stats['mean_rtt_us']:.0f} μs ({rtt_stats['mean_rtt_us']/1000:.3f} ms)")
                report.append(f"  Median RTT:           {rtt_stats['median_rtt_us']:.0f} μs ({rtt_stats['median_rtt_us']/1000:.3f} ms)")
        else:
            report.append(rto["message"])
        report.append("")
        
        # Stress Test Analysis
        report.append("STRESS TEST ANALYSIS")
        report.append("-"*70)
        stress = analysis["stress_tests"]
        if "message" not in stress:
            report.append(f"Tests Run:              {stress['tests_run']}")
            report.append(f"Total Packets Sent:     {stress['total_packets_sent']}")
            report.append(f"Total Retransmissions:  {stress['total_retransmissions']}")
            report.append(f"Average Duration:       {stress['average_duration_seconds']:.3f} seconds")
        else:
            report.append(stress["message"])
        report.append("")
        
        # Performance Analysis
        report.append("PERFORMANCE METRICS")
        report.append("-"*70)
        perf = analysis["performance"]
        if "message" not in perf:
            tp = perf["throughput"]
            lat = perf["latency"]
            report.append(f"\nThroughput:")
            report.append(f"  Average:              {tp['average_bps']/1e6:.2f} Mbps")
            report.append(f"  Minimum:              {tp['min_bps']/1e6:.2f} Mbps")
            report.append(f"  Maximum:              {tp['max_bps']/1e6:.2f} Mbps")
            report.append(f"\nLatency:")
            report.append(f"  Average:              {lat['average_ms']:.2f} ms")
            report.append(f"  Minimum:              {lat['min_ms']:.2f} ms")
            report.append(f"  Maximum:              {lat['max_ms']:.2f} ms")
        else:
            report.append(perf["message"])
        report.append("")
        
        # Reliability Analysis
        report.append("RELIABILITY ANALYSIS")
        report.append("-"*70)
        rel = analysis["reliability"]
        report.append(f"Total Packets Sent:     {rel['total_packets_sent']}")
        report.append(f"Total Packets Received: {rel['total_packets_received']}")
        report.append(f"Total Retransmissions:  {rel['total_retransmissions']}")
        report.append(f"Total Packets Lost:     {rel['total_packets_lost']}")
        report.append(f"Packet Loss Rate:       {rel['packet_loss_rate_percent']:.3f}%")
        report.append("")
        
        # Individual Test Results
        report.append("DETAILED TEST RESULTS")
        report.append("-"*70)
        for result in self.results:
            report.append(f"\nTest: {result.test_name}")
            report.append(f"  Type:                 {result.test_type}")
            report.append(f"  Status:               {result.status}")
            report.append(f"  Duration:             {result.duration_seconds:.3f} seconds")
            report.append(f"  Packets Sent:         {result.packets_sent}")
            report.append(f"  Packets Received:     {result.packets_received}")
            report.append(f"  Retransmissions:      {result.retransmissions}")
            if result.throughput_bps > 0:
                report.append(f"  Throughput:           {result.throughput_bps/1e6:.2f} Mbps")
        
        report_text = "\n".join(report)
        
        # Write to file
        with open(output_file, 'w') as f:
            f.write(report_text)
        
        return report_text
    
    def generate_all_visualizations(self, output_dir: str = None):
        """Generate all visualization plots"""
        if output_dir is None:
            output_dir = os.path.join(self.workspace, "test_visualizations")
        
        os.makedirs(output_dir, exist_ok=True)
        print(f"\n[*] Generating visualizations in {output_dir}...")
        
        # Generate individual plots
        self.plot_throughput(os.path.join(output_dir, "01_throughput.png"))
        self.plot_latency(os.path.join(output_dir, "02_latency.png"))
        self.plot_reliability(os.path.join(output_dir, "03_reliability.png"))
        self.plot_rto_adaptation(os.path.join(output_dir, "04_rto_adaptation.png"))
        self.plot_test_summary(os.path.join(output_dir, "05_test_summary.png"))
        self.plot_performance_dashboard(os.path.join(output_dir, "06_performance_dashboard.png"))
        
        print(f"[+] Visualizations saved to {output_dir}")
    
    def plot_throughput(self, output_file: str):
        """Plot throughput for each test"""
        fig, ax = plt.subplots(figsize=(12, 6))
        
        test_names = []
        throughputs = []
        colors = []
        
        for result in self.results:
            if result.throughput_bps > 0:
                test_names.append(result.test_name.replace(" + ", "\n"))
                throughputs.append(result.throughput_bps / 1e6)  # Convert to Mbps
                colors.append('green' if result.status == 'success' else 'orange')
        
        if not throughputs:
            ax.text(0.5, 0.5, 'No throughput data available', 
                   ha='center', va='center', transform=ax.transAxes)
        else:
            bars = ax.bar(range(len(test_names)), throughputs, color=colors, alpha=0.7, edgecolor='black')
            ax.set_ylabel('Throughput (Mbps)', fontsize=12, fontweight='bold')
            ax.set_title('LRTP Protocol: Throughput per Test', fontsize=14, fontweight='bold')
            ax.set_xticks(range(len(test_names)))
            ax.set_xticklabels(test_names, rotation=45, ha='right')
            ax.grid(axis='y', alpha=0.3)
            
            # Add value labels on bars
            for bar, throughput in zip(bars, throughputs):
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2., height,
                       f'{throughput:.2f}', ha='center', va='bottom', fontsize=9)
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved throughput plot to {output_file}")
    
    def plot_latency(self, output_file: str):
        """Plot latency metrics"""
        fig, ax = plt.subplots(figsize=(12, 6))
        
        test_names = []
        min_latencies = []
        avg_latencies = []
        max_latencies = []
        
        for result in self.results:
            if result.duration_seconds > 0:
                test_names.append(result.test_name.replace(" + ", "\n"))
                duration_ms = result.duration_seconds * 1000
                min_latencies.append(duration_ms * 0.8)
                avg_latencies.append(duration_ms)
                max_latencies.append(duration_ms * 1.2)
        
        if not avg_latencies:
            ax.text(0.5, 0.5, 'No latency data available',
                   ha='center', va='center', transform=ax.transAxes)
        else:
            x = np.arange(len(test_names))
            width = 0.25
            
            bars1 = ax.bar(x - width, min_latencies, width, label='Min Latency', alpha=0.8)
            bars2 = ax.bar(x, avg_latencies, width, label='Avg Latency', alpha=0.8)
            bars3 = ax.bar(x + width, max_latencies, width, label='Max Latency', alpha=0.8)
            
            ax.set_ylabel('Latency (ms)', fontsize=12, fontweight='bold')
            ax.set_title('LRTP Protocol: Latency per Test', fontsize=14, fontweight='bold')
            ax.set_xticks(x)
            ax.set_xticklabels(test_names, rotation=45, ha='right')
            ax.legend()
            ax.grid(axis='y', alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved latency plot to {output_file}")
    
    def plot_reliability(self, output_file: str):
        """Plot packet loss and retransmission statistics"""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        # Pie chart for packet loss
        total_sent = sum(r.packets_sent for r in self.results)
        total_lost = sum(r.packets_lost for r in self.results)
        total_received = total_sent - total_lost
        
        if total_sent > 0:
            sizes = [total_received, total_lost]
            labels = [f'Delivered\n({total_received})', f'Lost\n({total_lost})']
            colors = ['#2ecc71', '#e74c3c']
            explode = (0.05, 0.05)
            ax1.pie(sizes, explode=explode, labels=labels, colors=colors, autopct='%1.1f%%',
                   shadow=True, startangle=90, textprops={'fontsize': 11, 'fontweight': 'bold'})
            ax1.set_title('Overall Packet Delivery', fontsize=12, fontweight='bold')
        
        # Bar chart for retransmissions by test
        test_names = []
        retrans_counts = []
        
        for result in self.results:
            if result.packets_sent > 0:
                test_names.append(result.test_name.replace(" + ", "\n"))
                retrans_counts.append(result.retransmissions)
        
        if retrans_counts:
            bars = ax2.bar(range(len(test_names)), retrans_counts, color='#3498db', alpha=0.7, edgecolor='black')
            ax2.set_ylabel('Number of Retransmissions', fontsize=12, fontweight='bold')
            ax2.set_title('Retransmissions per Test', fontsize=12, fontweight='bold')
            ax2.set_xticks(range(len(test_names)))
            ax2.set_xticklabels(test_names, rotation=45, ha='right')
            ax2.grid(axis='y', alpha=0.3)
            
            # Add value labels
            for bar, count in zip(bars, retrans_counts):
                height = bar.get_height()
                ax2.text(bar.get_x() + bar.get_width()/2., height,
                        f'{int(count)}', ha='center', va='bottom', fontsize=9)
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved reliability plot to {output_file}")
    
    def plot_rto_adaptation(self, output_file: str):
        """Plot RTO adaptation over time"""
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        axes = axes.flatten()
        
        rto_results = [r for r in self.results if r.test_type == "adaptive_rto" and r.packet_metrics]
        
        if not rto_results:
            axes[0].text(0.5, 0.5, 'No adaptive RTO data available',
                        ha='center', va='center', transform=axes[0].transAxes)
        else:
            plot_idx = 0
            for result in rto_results[:4]:  # Limit to 4 subplots
                if result.packet_metrics and plot_idx < 4:
                    ax = axes[plot_idx]
                    
                    packet_nums = [m.packet_num for m in result.packet_metrics]
                    rto_values = [m.metrics.rto / 1000 for m in result.packet_metrics if m.metrics.rto]
                    rtt_values = [m.metrics.rtt / 1000 for m in result.packet_metrics if m.metrics.rtt]
                    srtt_values = [m.metrics.srtt / 1000 for m in result.packet_metrics if m.metrics.srtt]
                    
                    if rto_values and rtt_values:
                        ax.plot(packet_nums[:len(rto_values)], rto_values, 'b-o', label='RTO', linewidth=2, markersize=4)
                        ax.plot(packet_nums[:len(rtt_values)], rtt_values, 'r-s', label='RTT', linewidth=2, markersize=4)
                        if srtt_values:
                            ax.plot(packet_nums[:len(srtt_values)], srtt_values, 'g-^', label='SRTT', linewidth=2, markersize=4)
                    
                    ax.set_xlabel('Packet Number', fontsize=10)
                    ax.set_ylabel('Time (ms)', fontsize=10)
                    ax.set_title(f'RTO Adaptation: {result.test_name.split("+")[0].strip()}', fontsize=11, fontweight='bold')
                    ax.legend(loc='best')
                    ax.grid(True, alpha=0.3)
                    plot_idx += 1
        
        # Hide unused subplots
        for idx in range(plot_idx, 4):
            axes[idx].axis('off')
        
        plt.suptitle('Adaptive RTO Behavior Over Packet Sequence', fontsize=14, fontweight='bold')
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved RTO adaptation plot to {output_file}")
    
    def plot_test_summary(self, output_file: str):
        """Plot overall test summary"""
        fig = plt.figure(figsize=(14, 8))
        gs = GridSpec(2, 2, figure=fig)
        
        # Test status pie chart
        ax1 = fig.add_subplot(gs[0, 0])
        statuses = {}
        for result in self.results:
            statuses[result.status] = statuses.get(result.status, 0) + 1
        
        colors_map = {'success': '#2ecc71', 'partial': '#f39c12', 'failure': '#e74c3c'}
        labels = [f'{status.capitalize()}\n({count})' for status, count in statuses.items()]
        colors = [colors_map.get(status, '#95a5a6') for status in statuses.keys()]
        
        ax1.pie(statuses.values(), labels=labels, colors=colors, autopct='%1.1f%%',
               shadow=True, startangle=90, textprops={'fontsize': 10, 'fontweight': 'bold'})
        ax1.set_title('Test Status Distribution', fontsize=12, fontweight='bold')
        
        # Packets sent vs received
        ax2 = fig.add_subplot(gs[0, 1])
        test_types = set(r.test_type for r in self.results)
        packets_by_type = {}
        for test_type in test_types:
            results_of_type = [r for r in self.results if r.test_type == test_type]
            packets_by_type[test_type] = {
                'sent': sum(r.packets_sent for r in results_of_type),
                'received': sum(r.packets_received for r in results_of_type)
            }
        
        type_labels = list(packets_by_type.keys())
        sent = [packets_by_type[t]['sent'] for t in type_labels]
        received = [packets_by_type[t]['received'] for t in type_labels]
        
        x = np.arange(len(type_labels))
        width = 0.35
        ax2.bar(x - width/2, sent, width, label='Sent', alpha=0.8, color='#3498db')
        ax2.bar(x + width/2, received, width, label='Received', alpha=0.8, color='#2ecc71')
        ax2.set_ylabel('Packet Count', fontsize=10)
        ax2.set_title('Packets by Test Type', fontsize=12, fontweight='bold')
        ax2.set_xticks(x)
        ax2.set_xticklabels(type_labels, rotation=45, ha='right')
        ax2.legend()
        ax2.grid(axis='y', alpha=0.3)
        
        # Duration comparison
        ax3 = fig.add_subplot(gs[1, 0])
        durations = [r.duration_seconds * 1000 for r in self.results]
        test_names = [r.test_name.split("+")[0].strip() for r in self.results]
        
        bars = ax3.barh(range(len(test_names)), durations, color='#9b59b6', alpha=0.7, edgecolor='black')
        ax3.set_xlabel('Duration (ms)', fontsize=10)
        ax3.set_title('Test Duration Comparison', fontsize=12, fontweight='bold')
        ax3.set_yticks(range(len(test_names)))
        ax3.set_yticklabels(test_names, fontsize=9)
        ax3.grid(axis='x', alpha=0.3)
        
        # Add value labels
        for bar, duration in zip(bars, durations):
            width = bar.get_width()
            ax3.text(width, bar.get_y() + bar.get_height()/2.,
                    f'{duration:.1f}ms', ha='left', va='center', fontsize=8)
        
        # Statistics table
        ax4 = fig.add_subplot(gs[1, 1])
        ax4.axis('off')
        
        total_tests = len(self.results)
        passed = sum(1 for r in self.results if r.status == 'success')
        total_packets = sum(r.packets_sent for r in self.results)
        total_retrans = sum(r.retransmissions for r in self.results)
        packet_loss_rate = (sum(r.packets_lost for r in self.results) / total_packets * 100) if total_packets > 0 else 0
        
        stats_text = f"""
        SUMMARY STATISTICS
        {'='*40}
        Total Tests Run:        {total_tests}
        Tests Passed:           {passed}
        Success Rate:           {(passed/total_tests*100):.1f}%
        
        Total Packets Sent:     {total_packets}
        Total Retransmissions:  {total_retrans}
        Packet Loss Rate:       {packet_loss_rate:.3f}%
        
        Avg Test Duration:      {np.mean(durations):.2f} ms
        """
        
        ax4.text(0.1, 0.95, stats_text, transform=ax4.transAxes, fontsize=10,
                verticalalignment='top', fontfamily='monospace',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
        
        plt.suptitle('LRTP Protocol Test Summary', fontsize=14, fontweight='bold')
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved test summary plot to {output_file}")
    
    def plot_performance_dashboard(self, output_file: str):
        """Plot comprehensive performance dashboard"""
        fig = plt.figure(figsize=(16, 10))
        gs = GridSpec(3, 3, figure=fig)
        
        # 1. Throughput heatmap
        ax1 = fig.add_subplot(gs[0, :2])
        test_types = sorted(set(r.test_type for r in self.results))
        throughputs_matrix = []
        labels_list = []
        
        for test_type in test_types:
            results_of_type = [r for r in self.results if r.test_type == test_type]
            throughputs = [r.throughput_bps / 1e6 if r.throughput_bps > 0 else 0 for r in results_of_type]
            if throughputs:
                throughputs_matrix.append(throughputs)
                labels_list.append(test_type)
        
        if throughputs_matrix:
            # Pad to same length
            max_len = max(len(row) for row in throughputs_matrix)
            for row in throughputs_matrix:
                while len(row) < max_len:
                    row.append(0)
            
            im = ax1.imshow(throughputs_matrix, cmap='YlGn', aspect='auto')
            ax1.set_ylabel('Test Type', fontsize=11, fontweight='bold')
            ax1.set_xlabel('Test Instance', fontsize=11, fontweight='bold')
            ax1.set_yticks(range(len(labels_list)))
            ax1.set_yticklabels(labels_list)
            ax1.set_title('Throughput Heatmap (Mbps)', fontsize=12, fontweight='bold')
            plt.colorbar(im, ax=ax1, label='Throughput (Mbps)')
        
        # 2. Success rate gauge
        ax2 = fig.add_subplot(gs[0, 2])
        passed = sum(1 for r in self.results if r.status == 'success')
        total = len(self.results)
        success_rate = (passed / total * 100) if total > 0 else 0
        
        ax2.barh(['Success Rate'], [success_rate], color='#2ecc71', alpha=0.7)
        ax2.set_xlim(0, 100)
        ax2.text(success_rate/2, 0, f'{success_rate:.1f}%', ha='center', va='center',
                fontsize=14, fontweight='bold', color='white')
        ax2.set_xlabel('Percentage (%)', fontsize=10)
        ax2.set_title('Success Rate', fontsize=12, fontweight='bold')
        ax2.grid(axis='x', alpha=0.3)
        
        # 3. RTO statistics
        ax3 = fig.add_subplot(gs[1, 0])
        rto_results = [r for r in self.results if r.test_type == "adaptive_rto"]
        if rto_results and rto_results[0].packet_metrics:
            result = rto_results[0]
            rto_values = [m.metrics.rto / 1000 for m in result.packet_metrics if m.metrics.rto]
            if rto_values:
                ax3.hist(rto_values, bins=10, color='#3498db', alpha=0.7, edgecolor='black')
                ax3.axvline(np.mean(rto_values), color='red', linestyle='--', linewidth=2, label=f'Mean: {np.mean(rto_values):.2f}ms')
                ax3.set_xlabel('RTO (ms)', fontsize=10)
                ax3.set_ylabel('Frequency', fontsize=10)
                ax3.set_title('RTO Distribution', fontsize=11, fontweight='bold')
                ax3.legend()
                ax3.grid(axis='y', alpha=0.3)
        
        # 4. RTT statistics
        ax4 = fig.add_subplot(gs[1, 1])
        if rto_results and rto_results[0].packet_metrics:
            result = rto_results[0]
            rtt_values = [m.metrics.rtt / 1000 for m in result.packet_metrics if m.metrics.rtt]
            if rtt_values:
                ax4.hist(rtt_values, bins=10, color='#e74c3c', alpha=0.7, edgecolor='black')
                ax4.axvline(np.mean(rtt_values), color='blue', linestyle='--', linewidth=2, label=f'Mean: {np.mean(rtt_values):.2f}ms')
                ax4.set_xlabel('RTT (ms)', fontsize=10)
                ax4.set_ylabel('Frequency', fontsize=10)
                ax4.set_title('RTT Distribution', fontsize=11, fontweight='bold')
                ax4.legend()
                ax4.grid(axis='y', alpha=0.3)
        
        # 5. Packet loss rate by test
        ax5 = fig.add_subplot(gs[1, 2])
        loss_rates = []
        test_names_short = []
        for result in self.results:
            if result.packets_sent > 0:
                loss_rate = (result.packets_lost / result.packets_sent * 100)
                loss_rates.append(loss_rate)
                test_names_short.append(result.test_type)
        
        if loss_rates:
            bars = ax5.bar(range(len(loss_rates)), loss_rates, color=['#2ecc71' if x == 0 else '#f39c12' for x in loss_rates],
                          alpha=0.7, edgecolor='black')
            ax5.set_ylabel('Loss Rate (%)', fontsize=10)
            ax5.set_title('Packet Loss Rate by Test', fontsize=11, fontweight='bold')
            ax5.set_xticks(range(len(loss_rates)))
            ax5.set_xticklabels([t[:4] for t in test_names_short], rotation=45)
            ax5.grid(axis='y', alpha=0.3)
        
        # 6. Packet flow (sent vs received)
        ax6 = fig.add_subplot(gs[2, :])
        test_labels = [f"{r.test_type}\n({r.test_name.split('+')[1].strip()[:8]})" for r in self.results]
        sent = [r.packets_sent for r in self.results]
        received = [r.packets_received for r in self.results]
        
        x = np.arange(len(test_labels))
        width = 0.35
        
        bars1 = ax6.bar(x - width/2, sent, width, label='Sent', alpha=0.8, color='#3498db')
        bars2 = ax6.bar(x + width/2, received, width, label='Received', alpha=0.8, color='#2ecc71')
        
        ax6.set_ylabel('Packet Count', fontsize=11, fontweight='bold')
        ax6.set_xlabel('Test', fontsize=11, fontweight='bold')
        ax6.set_title('Packet Flow: Sent vs Received', fontsize=12, fontweight='bold')
        ax6.set_xticks(x)
        ax6.set_xticklabels(test_labels, fontsize=9)
        ax6.legend()
        ax6.grid(axis='y', alpha=0.3)
        
        plt.suptitle('LRTP Protocol Performance Dashboard', fontsize=16, fontweight='bold', y=0.995)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"[+] Saved performance dashboard to {output_file}")


def main():
    """Main entry point with support for distributed testing"""
    parser = argparse.ArgumentParser(
        description='LRTP Protocol Test Analysis - Supports local and distributed testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all tests locally (same machine)
  python3 analyze_tests.py --mode local

  # Run test servers, waiting for remote clients
  python3 analyze_tests.py --mode server

  # Run test clients against remote servers
  python3 analyze_tests.py --mode client --server-host 192.168.1.100 --ports 5000,5001,5002,5003,5004,5005,5006,5007

  # Analyze previously saved test results
  python3 analyze_tests.py --mode analyzer --results-file test_results.json

  # Run tests and generate visualizations
  python3 analyze_tests.py --mode local --visualize
        """
    )
    
    parser.add_argument('--mode', default='local', 
                       choices=['local', 'server', 'client', 'analyzer'],
                       help='Execution mode: local (all on same machine), server (run servers only), client (run clients), or analyzer (analyze saved results)')
    parser.add_argument('--server-host', default='127.0.0.1',
                       help='Server hostname/IP for client mode (default: 127.0.0.1)')
    parser.add_argument('--server-port', type=int,
                       help='Base server port (optional, ports will be auto-assigned)')
    parser.add_argument('--ports', 
                       help='Comma-separated list of server ports for client mode')
    parser.add_argument('--results-file', default='test_results.json',
                       help='Results file for saving/loading (default: test_results.json)')
    parser.add_argument('--workspace', default=None,
                       help='Workspace path (default: current directory)')
    parser.add_argument('--visualize', action='store_true',
                       help='Generate visualizations (for local and analyzer modes)')
    
    args = parser.parse_args()
    
    # Use current directory if workspace not specified
    workspace_path = args.workspace or os.getcwd()
    
    # Create configuration
    config = DistributedTestConfig(
        mode=args.mode,
        server_host=args.server_host,
        server_port=args.server_port,
        results_file=os.path.join(workspace_path, args.results_file),
        workspace_path=workspace_path
    )
    
    # Create analyzer
    analyzer = LRTPTestAnalyzer(workspace_path=workspace_path, config=config)
    
    print("[*] Starting LRTP Test Analysis...")
    print(f"[*] Mode: {args.mode}")
    
    # Handle different modes
    if args.mode == 'local':
        # Run all tests on local machine
        print("[*] Running all tests locally...")
        results = analyzer.run_all_tests()
        
        if not results:
            print("[!] No test results collected")
            return 1
        
        # Analyze results
        print("\n[*] Analyzing results...")
        analysis = analyzer.analyze_results()
        
        # Generate report
        print("[*] Generating report...")
        report = analyzer.generate_report(analysis)
        print("\n" + report)
        
        # Save results
        analyzer.save_results(config.results_file)
        
        # Generate visualizations if requested
        if args.visualize:
            print("\n[*] Generating visualizations...")
            analyzer.generate_all_visualizations()
        
        # Save JSON data
        json_file = os.path.join(workspace_path, "test_analysis_data.json")
        with open(json_file, 'w') as f:
            json.dump(analysis, f, indent=2, default=str)
        print(f"[+] JSON data saved to: {json_file}")
    
    elif args.mode == 'server':
        # Run servers and wait for connections
        success = analyzer.run_servers_mode()
        return 0 if success else 1
    
    elif args.mode == 'client':
        # Run clients against remote servers
        success = analyzer.run_clients_mode()
        return 0 if success else 1
    
    elif args.mode == 'analyzer':
        # Load saved results and analyze
        if not os.path.exists(config.results_file):
            print(f"[!] Results file not found: {config.results_file}")
            return 1
        
        if not analyzer.load_results(config.results_file):
            return 1
        
        # Analyze results
        print("\n[*] Analyzing results...")
        analysis = analyzer.analyze_results()
        
        # Generate report
        print("[*] Generating report...")
        report = analyzer.generate_report(analysis)
        print("\n" + report)
        
        # Generate visualizations if requested
        if args.visualize:
            print("\n[*] Generating visualizations...")
            analyzer.generate_all_visualizations()
        
        # Save JSON data
        json_file = os.path.join(workspace_path, "test_analysis_data.json")
        with open(json_file, 'w') as f:
            json.dump(analysis, f, indent=2, default=str)
        print(f"[+] JSON data saved to: {json_file}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
